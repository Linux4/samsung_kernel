// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/videodev2_exynos_media.h>
#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"
#include "is-hw-yuvp.h"
#include "is-stripe.h"

static int __ischain_yuvp_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_LVN_YUVP_YUV:
		*pindex = PARAM_YUVP_YUV;
		ret = 2;
		break;
	case IS_LVN_YUVP_DRC:
		*pindex = PARAM_YUVP_DRC;
		ret = 1;
		break;
	case IS_LVN_YUVP_CLAHE:
		*pindex = PARAM_YUVP_CLAHE;
		ret = 1;
		break;
	case IS_LVN_YUVP_SEG:
		*pindex = PARAM_YUVP_SEG;
		ret = 1;
		break;
	case IS_LVN_YUVP_SVHIST:
		*pindex = PARAM_YUVP_SVHIST;
		ret = 2;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int __yuvp_dma_out_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_dma_output *dma_output;
	struct is_crop *incrop, *otcrop;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_msb = (DMA_INOUT_BIT_WIDTH_10BIT - 1);
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_order, hw_plane = 2;
	u32 width, flag_extra;
	int ret = 0;

	FIMC_BUG(!frame);
	FIMC_BUG(!node);

	dma_output = is_itf_g_param(device, frame, pindex);

	if (dma_output->cmd != node->request)
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->cmd, node->request);
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	set_bit(pindex, pmap);

	if (!node->request)
		return 0;

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		merr("incrop is NULL", device);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		merr("otcrop is NULL", device);
		return -EINVAL;
	}

	mdbgs_ischain(4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, otcrop->w, otcrop->h);
	width = incrop->w;

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	/* FIXME */
	/* hw_bitwidth = fmt->hw_bitwidth; */
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	if (dma_output->format != fmt->hw_format)
		mlvinfo("[F%d]WDMA format: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->format, fmt->hw_format);
	if (dma_output->bitwidth != fmt->hw_bitwidth)
		mlvinfo("[F%d]WDMA bitwidth: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->bitwidth, fmt->hw_bitwidth);
	if (dma_output->sbwc_type != hw_sbwc)
		mlvinfo("[F%d][%d] WDMA sbwc_type: %d -> %d\n", device,
			node->vid, frame->fcount, pindex,
			dma_output->sbwc_type, hw_sbwc);
	if (!frame->stripe_info.region_num && ((dma_output->width != otcrop->w) ||
		(dma_output->height != otcrop->h)))
		mlvinfo("[F%d][%d] WDMA otcrop[%d, %d, %d, %d]\n", device,
			node->vid, frame->fcount, pindex,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->bitwidth = fmt->hw_bitwidth;
	dma_output->msb = fmt->hw_bitwidth - 1;
	dma_output->format = fmt->hw_format;
	dma_output->order = fmt->hw_order;
	dma_output->plane = fmt->hw_plane;

	dma_output->dma_crop_offset_x = incrop->x;
	dma_output->dma_crop_offset_y = incrop->y;
	dma_output->dma_crop_width = incrop->w;
	dma_output->dma_crop_height = incrop->h;

	dma_output->width = otcrop->w;
	dma_output->height = otcrop->h;

	dma_output->stride_plane0 = max(node->bytesperline[0], (otcrop->w * fmt->bitsperpixel[0]) / BITS_PER_BYTE);
	dma_output->stride_plane1 = max(node->bytesperline[1], (otcrop->w * fmt->bitsperpixel[1]) / BITS_PER_BYTE);

	return ret;
}

static int __yuvp_otf_cfg(struct is_device_ischain *device,
		struct is_group *group,
		struct is_frame *ldr_frame,
		struct camera2_node *node,
		struct is_stripe_info *stripe_info,
		IS_DECLARE_PMAP(pmap))
{
	struct param_otf_input *otf_in;
	struct param_otf_output *otf_out;
	struct is_crop *otcrop;

	otf_in = is_itf_g_param(device, ldr_frame, PARAM_YUVP_OTF_INPUT);
	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state))
		otf_in->cmd = OTF_INPUT_COMMAND_DISABLE;
	else
		otf_in->cmd = OTF_INPUT_COMMAND_ENABLE;

	set_bit(PARAM_YUVP_OTF_INPUT, pmap);

	if (!otf_in->cmd)
		return 0;

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		merr("otcrop is NULL", device);
		return -EINVAL;
	}

	if (!ldr_frame->stripe_info.region_num &&
	    ((otf_in->width != otcrop->w) || (otf_in->height != otcrop->h)))
		mlvinfo("[F%d] OTF incrop[%d, %d, %d, %d]\n",
				device, IS_VIDEO_YUVP, ldr_frame->fcount,
				otcrop->x, otcrop->y,
				otcrop->w, otcrop->h);

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num)
		otf_in->width = ldr_frame->stripe_info.out.crop_width;
	else
		otf_in->width = otcrop->w;
	otf_in->height = otcrop->h;
	otf_in->offset_x = 0;
	otf_in->offset_y = 0;
	otf_in->physical_width = otf_in->width;
	otf_in->physical_height = otf_in->height;
	otf_in->format = OTF_INPUT_FORMAT_YUV422;
	otf_in->bayer_crop_offset_x = 0;
	otf_in->bayer_crop_offset_y = 0;
	otf_in->bayer_crop_width = otf_in->width;
	otf_in->bayer_crop_height = otf_in->height;

	otf_out = is_itf_g_param(device, ldr_frame, PARAM_YUVP_OTF_OUTPUT);
	otf_out->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	set_bit(PARAM_YUVP_OTF_OUTPUT, pmap);

	mdbgs_ischain(4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, otf_in->width, otf_in->height);

	return 0;
}

static int __yuvp_dma_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct is_fmt *fmt;
	struct param_dma_input *dma_input;
	struct is_crop *incrop, *otcrop;
	struct is_group *group;
	u32 hw_format = DMA_INOUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	u32 hw_msb = (DMA_INOUT_BIT_WIDTH_10BIT - 1);
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 hw_order, hw_plane = 2;
	u32 width, height, flag_extra;
	int ret = 0;

	set_bit(pindex, pmap);

	group = &device->group_yuvp;

	dma_input = is_itf_g_param(device, frame, pindex);

	if (!IS_ENABLED(IRTA_CALL)) {
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
		return 0;
	}

	if (pindex == PARAM_YUVP_DMA_INPUT && test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
	} else {
		if (dma_input->cmd != node->request)
			mlvinfo("[F%d] RDMA enable: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->cmd, node->request);
		dma_input->cmd = node->request;

		if (!dma_input->cmd)
			return 0;
	}

	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		merr("incrop is NULL", device);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		merr("otcrop is NULL", device);
		return -EINVAL;
	}

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && frame->stripe_info.region_num)
		width = frame->stripe_info.out.crop_width;
	else
		width = otcrop->w;

	height = otcrop->h;

	mdbgs_ischain(4, "%s : otcrop (w x h) = (%d x %d)\n", device, __func__, width, height);

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixel format(0x%x) is not found", device, node->pixelformat);
		return -EINVAL;
	}

	hw_format = fmt->hw_format;
	/* FIXME */
	/* hw_bitwidth = fmt->hw_bitwidth; */
	hw_msb = hw_bitwidth - 1;
	hw_order = fmt->hw_order;
	hw_plane = fmt->hw_plane;
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK)
			>> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && frame->stripe_info.region_num &&
	    (node->vid == IS_VIDEO_YUVP))
		is_ischain_g_stripe_cfg(frame, node, incrop, incrop, otcrop);

	if (dma_input->cmd) {
		if (dma_input->sbwc_type != hw_sbwc)
			mlvinfo("[F%d]RDMA sbwc_type: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->sbwc_type, hw_sbwc);
		if (!frame->stripe_info.region_num && ((dma_input->width != width) ||
				(dma_input->height != height)))
			mlvinfo("[F%d]RDMA incrop[%d, %d, %d, %d]\n", device,
				node->vid, frame->fcount,
				incrop->x, incrop->y, width, height);
	}

	/* DRC and SV hist have own format enum */
	switch (node->vid) {
	/* RDMA */
	case IS_LVN_YUVP_YUV:
		hw_plane = DMA_INOUT_PLANE_2;
		break;
	case IS_LVN_YUVP_DRC:
		hw_format = DMA_INOUT_FORMAT_DRCGRID;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	case IS_LVN_YUVP_CLAHE:
	case IS_LVN_YUVP_SVHIST:
		hw_format = DMA_INOUT_FORMAT_SVHIST;
		hw_plane = DMA_INOUT_PLANE_1;
		break;
	case IS_VIDEO_YNS_NUM:
		hw_format = DMA_INOUT_FORMAT_NOISE;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
		hw_plane = DMA_INOUT_PLANE_1;
		break;
	case IS_LVN_YUVP_SEG:
		hw_format = DMA_INOUT_FORMAT_SEGCONF;
		hw_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		hw_msb = hw_bitwidth - 1;
		break;
	default:
		if (dma_input->bitwidth != hw_bitwidth)
			mlvinfo("[F%d]RDMA bitwidth: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->bitwidth, hw_bitwidth);

		if (dma_input->format != hw_format)
			mlvinfo("[F%d]RDMA format: %d -> %d\n", device,
				node->vid, frame->fcount,
				dma_input->format, hw_format);
		break;
	}

	dma_input->sbwc_type= hw_sbwc;
	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = hw_msb;
	dma_input->order = hw_order;
	dma_input->sbwc_type = hw_sbwc;
	dma_input->plane = hw_plane;
	dma_input->width = width;
	dma_input->height = height;
	dma_input->dma_crop_offset = (incrop->x << 16) | (incrop->y << 0);
	dma_input->dma_crop_width = width;
	dma_input->dma_crop_height = height;
	dma_input->bayer_crop_offset_x = otcrop->x;
	dma_input->bayer_crop_offset_y = otcrop->y;
	dma_input->bayer_crop_width = width;
	dma_input->bayer_crop_height = height;
	dma_input->stride_plane0 = width;
	dma_input->stride_plane1 = width;
	dma_input->stride_plane2 = width;

	return ret;
}

static int __yuvp_stripe_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	struct param_stripe_input *stripe_input;
	struct is_crop *otcrop;
	int i;

	set_bit(pindex, pmap);

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}

	stripe_input = is_itf_g_param(device, frame, pindex);
	if (frame->stripe_info.region_num) {
		stripe_input->index = frame->stripe_info.region_id;
		stripe_input->total_count = frame->stripe_info.region_num;
		if (!frame->stripe_info.region_id) {
			stripe_input->stripe_roi_start_pos_x[0] = 0;
			for (i = 1; i < frame->stripe_info.region_num; ++i)
				stripe_input->stripe_roi_start_pos_x[i] =
					frame->stripe_info.h_pix_num[i - 1];
		}

		stripe_input->left_margin = frame->stripe_info.out.left_margin;
		stripe_input->right_margin = frame->stripe_info.out.right_margin;
		stripe_input->full_width = otcrop->w;
		stripe_input->full_height = otcrop->h;
		stripe_input->start_pos_x = frame->stripe_info.in.crop_x +
						frame->stripe_info.in.offset_x;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}

	pablo_update_repeat_param(frame, stripe_input);

	return 0;
}

static void __yuvp_control_cfg(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame,
	IS_DECLARE_PMAP(pmap))
{
	struct param_control *control;

	control = is_itf_g_param(device, frame, PARAM_YUVP_CONTROL);
	if (test_bit(IS_GROUP_STRGEN_INPUT, &group->state))
		control->strgen = CONTROL_COMMAND_START;
	else
		control->strgen = CONTROL_COMMAND_STOP;

	set_bit(PARAM_YUVP_CONTROL, pmap);
}

static int is_ischain_yuvp_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct yuvp_param *yuvp_param;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	IS_DECLARE_PMAP(pmap);
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	dma_addr_t *targetAddr;
	u64 *targetAddr_k;
	u32 n, p, pindex = 0;
	int dma_type;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_yuvp;
	leader = subdev->leader;
	IS_INIT_PMAP(pmap);
	yuvp_param = &device->is_region->parameter.yuvp;

	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		__yuvp_control_cfg(device, group, frame, pmap);

		out_node = &frame->shot_ext->node_group.leader;

		ret = __yuvp_otf_cfg(device, group, frame, out_node,
				&frame->stripe_info, pmap);
		if (ret) {
			mlverr("[F%d] otf_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		ret = __yuvp_dma_in_cfg(device, subdev, frame, out_node,
				PARAM_YUVP_DMA_INPUT, pmap);
		if (ret) {
			mlverr("[F%d] dma_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		ret = __yuvp_stripe_in_cfg(device, subdev, frame, out_node,
				PARAM_YUVP_STRIPE_INPUT, pmap);
		if (ret) {
			mlverr("[F%d] strip_in_cfg fail. ret %d", device, node->vid,
					frame->fcount, ret);
			return ret;
		}

		for (n = 0; n < CAPTURE_NODE_MAX; n++) {
			cap_node = &frame->shot_ext->node_group.capture[n];

			if (!cap_node->vid)
				continue;

			dma_type = __ischain_yuvp_slot(cap_node, &pindex);
			if (dma_type == 1) {
				ret = __yuvp_dma_in_cfg(device, subdev, frame,
						cap_node, pindex, pmap);
			} else if (dma_type == 2) {
				ret = __yuvp_dma_out_cfg(device, subdev, frame, cap_node,
						pindex, pmap);
			} else {
				if (IS_ENABLED(IRTA_CALL) &&
					cap_node->vid == IS_LVN_YUVP_RTA_INFO &&
					!cap_node->request) {
					mlverr("[F%d] rta info node is disabled", device, cap_node->vid,
						frame->fcount);
					ret = -EINVAL;
					goto p_err;
				}
			}

			if (ret) {
				mlverr("[F%d] dma_%s_cfg error\n", device, cap_node->vid,
						frame->fcount,
						(dma_type == 1) ? "in" : "out");
				return ret;
			}
		}

		for (n = 0; n < CAPTURE_NODE_MAX; n++) {
			sframe = &frame->cap_node.sframe[n];
			if (!sframe->id)
				continue;

			if ((sframe->id >= IS_LVN_MCSC_P0) &&
					(sframe->id <= IS_LVN_MCSC_P5))
				continue;

			targetAddr = NULL;
			targetAddr_k = NULL;
			ret = is_hw_get_capture_slot(frame, &targetAddr,
					&targetAddr_k, sframe->id);
			if (ret) {
				mrerr("Invalid capture slot(%d)", device,
						frame, sframe->id);
				return -EINVAL;
			}

			if (targetAddr) {
				for (p = 0; p < sframe->num_planes; p++)
					targetAddr[p] = sframe->dva[p];
			}

			if (targetAddr_k) {
				for (p = 0; p < sframe->num_planes; p++)
					targetAddr_k[p] = sframe->kva[p];
			}
		}

		ret = is_itf_s_param(device, frame, pmap);
		if (ret) {
			mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
			goto p_err;
		}

		return 0;
	}

p_err:
	return ret;
}

static int is_ischain_yuvp_get(struct is_subdev *subdev,
			       struct is_device_ischain *idi,
			       struct is_frame *frame,
			       enum pablo_subdev_get_type type,
			       void *result)
{
	struct camera2_node *node;
	struct is_crop *outcrop;

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

static const struct is_subdev_ops is_subdev_yuvp_ops = {
	.bypass	= NULL,
	.cfg	= NULL,
	.tag	= is_ischain_yuvp_tag,
	.get	= is_ischain_yuvp_get,
};

const struct is_subdev_ops *pablo_get_is_subdev_yuvp_ops(void)
{
	return &is_subdev_yuvp_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_is_subdev_yuvp_ops);
