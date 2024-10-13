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

#include "is-device-ischain.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"
#include "is-hw.h"

#ifdef USE_VRA_OTF
static int is_ischain_mcs_bypass(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	bool bypass)
{
	int ret = 0;
	struct is_group *group;
	struct param_mcs_output *output;
	struct is_device_ischain *device;
	struct is_subdev *subdev = NULL, *subdev_temp = NULL, *temp;
	IS_DECLARE_PMAP(pmap);

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);

	group = &device->group_mcs;
	IS_INIT_PMAP(pmap);

	list_for_each_entry_safe(subdev_temp, temp, &group->subdev_list, list) {
		if (subdev_temp == &group->leader)
			continue;

		if (test_bit(IS_SUBDEV_START, &subdev_temp->state)) {
			subdev = subdev_temp;
			goto find_subdev;
		}
	}

find_subdev:
	if (!subdev) {
		mgerr("subdev is NULL\n", device, group);
		goto p_err;
	}

	output = is_itf_g_param(device, frame, subdev->param_dma_ot);

	if (bypass)
		output->otf_cmd = OTF_OUTPUT_COMMAND_DISABLE;
	else
		output->otf_cmd = OTF_OUTPUT_COMMAND_ENABLE;

	set_bit(subdev->param_dma_ot, pmap);

	ret = is_itf_s_param(device, frame, pmap);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

	minfo("VRA is connected at %s\n", device, group->junction->name);

p_err:
	return ret;
}
#endif

void __mcs_in_stripe_cfg(struct is_frame *frame,
		struct camera2_node *node,
		struct is_crop *incrop,
		struct is_crop *otcrop)
{
	u32 stripe_w, stripe_margin;
	u32 region_id = frame->stripe_info.region_id;

	stripe_margin = STRIPE_MARGIN_WIDTH;

	if (!region_id) {
		/* Left region */
		stripe_w = frame->stripe_info.out.h_pix_num[region_id];
	} else if (region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		stripe_w = frame->stripe_info.out.h_pix_num[region_id] - frame->stripe_info.out.h_pix_num[region_id - 1];
		stripe_w += stripe_margin;
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.out.h_pix_num[region_id - 1];
	}
	stripe_w += stripe_margin;
	incrop->w = stripe_w;

	/**
	 * Output crop configuration.
	 * No crop & scale.
	 */
	otcrop->x = 0;
	otcrop->y = 0;
	otcrop->w = incrop->w;
	otcrop->h = incrop->h;

	mlvdbgs(3, "[MCS][F%d] stripe_in_crop[%d][%d, %d, %d, %d]\n", frame, node->vid,
			frame->fcount, frame->stripe_info.region_id,
			incrop->x, incrop->y, incrop->w, incrop->h);
	mlvdbgs(3, "[MCS][F%d] stripe_ot_crop[%d][%d, %d, %d, %d]\n", frame, node->vid,
			frame->fcount, frame->stripe_info.region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
}

static int __ischain_mcsc_slot(struct camera2_node *node, u32 *pindex)
{
	int ret = 0;

	switch (node->vid) {
	case IS_VIDEO_M0P_NUM:
		*pindex = PARAM_MCS_OUTPUT0;
		ret = 2;
		break;
	case IS_VIDEO_M1P_NUM:
		*pindex = PARAM_MCS_OUTPUT1;
		ret = 2;
		break;
	case IS_VIDEO_M2P_NUM:
		*pindex = PARAM_MCS_OUTPUT2;
		ret = 2;
		break;
	case IS_VIDEO_M3P_NUM:
		*pindex = PARAM_MCS_OUTPUT3;
		ret = 2;
		break;
	case IS_VIDEO_M4P_NUM:
		*pindex = PARAM_MCS_OUTPUT4;
		ret = 2;
		break;
	case IS_VIDEO_M5P_NUM:
		*pindex = PARAM_MCS_OUTPUT5;
		ret = 2;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

int __mcsc_in_cfg(struct is_device_ischain *device,
	struct is_subdev *leader,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap))
{
	int i;
	struct is_group *group;
	struct param_control *control;
	struct param_mcs_input *mscs_input;
	struct param_stripe_input *stripe_input;
	struct is_crop *incrop, *otcrop;
	struct is_crop incrop_cfg, otcrop_cfg;
	struct is_fmt *fmt;

	FIMC_BUG(!device);
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!node);

	group = &device->group_mcs;
	incrop = (struct is_crop *)node->input.cropRegion;
	if (IS_NULL_CROP(incrop)) {
		mlverr("[MCS] incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("[MCS] otcrop is NULL", device, node->vid);
		return -EINVAL;
	}
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		mlverr("[MCS] fmt is NULL", device, node->vid);
		return -EINVAL;
	}

	mscs_input = is_itf_g_param(device, ldr_frame, pindex);
	stripe_input = is_itf_g_param(device, ldr_frame, PARAM_MCS_STRIPE_INPUT);
	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame && ldr_frame->stripe_info.region_num) {
		__mcs_in_stripe_cfg(ldr_frame, node, &incrop_cfg, &otcrop_cfg);
		if ((stripe_input->full_width != incrop->w) ||
		    (mscs_input->height != incrop->h))
			mlvinfo("[MCS][F%d] OTF incrop[%d, %d, %d, %d], region_num(%d)\n", device,
				node->vid, ldr_frame->fcount,
				incrop->x, incrop->y, incrop->w, incrop->h,
				ldr_frame->stripe_info.region_num);
	}

	if (ldr_frame && ldr_frame->stripe_info.region_num) {
		stripe_input->index = ldr_frame->stripe_info.region_id;
		stripe_input->total_count = ldr_frame->stripe_info.region_num;
		if (!ldr_frame->stripe_info.region_id) {
			stripe_input->left_margin = 0;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->stripe_roi_start_pos_x[0] = 0;
			for (i = 1; i <= ldr_frame->stripe_info.region_num; ++i)
				stripe_input->stripe_roi_start_pos_x[i] = ldr_frame->stripe_info.in.h_pix_num[i - 1];
		} else if (ldr_frame->stripe_info.region_id < ldr_frame->stripe_info.region_num - 1) {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
		} else {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = 0;
		}
		stripe_input->full_width = incrop->w;
		stripe_input->full_height = incrop->h;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}
	set_bit(PARAM_MCS_STRIPE_INPUT, pmap);

	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (mscs_input->otf_cmd  != OTF_INPUT_COMMAND_ENABLE)
		mlvinfo("[MCS][F%d] OTF enable: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mscs_input->otf_cmd, OTF_INPUT_COMMAND_ENABLE);
		mscs_input->otf_cmd = OTF_INPUT_COMMAND_ENABLE;
		mscs_input->dma_cmd = DMA_INPUT_COMMAND_DISABLE;
		if (!ldr_frame->stripe_info.region_num &&
			((mscs_input->width != otcrop_cfg.w) || (mscs_input->height != otcrop_cfg.h)))
			mlvinfo("[MCS][F%d] OTF incrop[%d, %d, %d, %d]\n", device,
				node->vid, ldr_frame->fcount,
				otcrop_cfg.x, otcrop_cfg.y, otcrop_cfg.w, otcrop_cfg.h);
		mscs_input->width = otcrop_cfg.w;
		mscs_input->height = otcrop_cfg.h;
	} else {
		mscs_input->otf_cmd = OTF_INPUT_COMMAND_DISABLE;
		mscs_input->dma_cmd = DMA_INPUT_COMMAND_ENABLE;
		mscs_input->width = leader->input.width;
		mscs_input->height = leader->input.height;
		mscs_input->dma_crop_offset_x = leader->input.canv.x
						+ incrop_cfg.x;
		mscs_input->dma_crop_offset_y = leader->input.canv.y
						+ incrop_cfg.y;
		mscs_input->dma_crop_width = incrop_cfg.w;
		mscs_input->dma_crop_height = incrop_cfg.h;

		mscs_input->dma_format = fmt->hw_format;
		mscs_input->dma_bitwidth = fmt->hw_bitwidth;
		mscs_input->dma_order = fmt->hw_order;
		mscs_input->plane = fmt->hw_plane;

		mscs_input->dma_stride_y = max(mscs_input->dma_crop_width * fmt->bitsperpixel[0] / BITS_PER_BYTE,
					node->bytesperline[0]);
		mscs_input->dma_stride_c = max(mscs_input->dma_crop_width * fmt->bitsperpixel[1] / BITS_PER_BYTE,
					node->bytesperline[1]);

	}

	mscs_input->otf_format = OTF_INPUT_FORMAT_YUV422;
	mscs_input->otf_bitwidth = OTF_INPUT_BIT_WIDTH_8BIT;
	mscs_input->otf_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	mscs_input->stripe_in_start_pos_x = stripe_input->stripe_roi_start_pos_x[stripe_input->index]
									- stripe_input->left_margin;
	mscs_input->stripe_roi_start_pos_x = stripe_input->stripe_roi_start_pos_x[stripe_input->index];
	mscs_input->stripe_roi_end_pos_x = stripe_input->stripe_roi_start_pos_x[stripe_input->index + 1];
	mscs_input->stripe_in_end_pos_x = stripe_input->stripe_roi_start_pos_x[stripe_input->index + 1]
									+ stripe_input->right_margin;
	set_bit(pindex, pmap);

	/* Configure Control */
	control = is_itf_g_param(device, ldr_frame, PARAM_MCS_CONTROL);
	control->cmd = CONTROL_COMMAND_START;

	set_bit(PARAM_MCS_CONTROL, pmap);

	return 0;
}

static int is_ischain_mcs_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct mcs_param *mcs_param;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	IS_DECLARE_PMAP(pmap);
	struct is_queue *queue;
	struct camera2_node *out_node = NULL;
	struct camera2_node *cap_node = NULL;
	struct is_sub_frame *sframe;
	dma_addr_t *targetAddr;
	u32 pindex = 0;
	int j, i, dma_type;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "MCSC TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	leader = subdev->leader;
	IS_INIT_PMAP(pmap);
	mcs_param = &device->is_region->parameter.mcs;

	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		queue = GET_SUBDEV_QUEUE(leader);
		if (!queue) {
			merr("queue is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		out_node = &frame->shot_ext->node_group.leader;
		/* The input of MCSC is always OTF */
		ret = __mcsc_in_cfg(device, subdev, frame, out_node,
				PARAM_MCS_INPUT, pmap);

		for (i = 0; i < CAPTURE_NODE_MAX; i++) {
			cap_node = &frame->shot_ext->node_group.capture[i];
			if (!cap_node->vid)
				continue;

			dma_type = __ischain_mcsc_slot(cap_node, &pindex);
			if (dma_type == 1)
				ret = __mcsc_in_cfg(device, subdev, frame,
						cap_node,
						pindex,
						pmap);
			else if (dma_type == 2)
				ret = __mcsc_dma_out_cfg(device, frame,
						cap_node,
						pindex,
						pmap,
						i);
			if (ret) {
				mrerr("__mcsc_%s_cfg is fail(%d)", device, frame,
					dma_type == 1 ? "in" : "dma_out", ret);
				goto p_err;
			}
		}

		for (i = 0; i < CAPTURE_NODE_MAX; i++) {
			sframe = &frame->cap_node.sframe[i];
			if (!sframe->id)
				continue;

			if ((sframe->id < IS_VIDEO_M0P_NUM) ||
					(sframe->id > IS_VIDEO_M5P_NUM))
				continue;

			targetAddr = NULL;
			ret = is_hw_get_capture_slot(frame, &targetAddr, NULL,
					sframe->id);
			if (ret) {
				mrerr("Invalid capture slot(%d)", device, frame,
						sframe->id);
				return -EINVAL;
			}

			if (!targetAddr)
				continue;

			for (j = 0; j < sframe->num_planes; j++)
				targetAddr[j] = sframe->dva[j];
		}

		ret = is_itf_s_param(device, frame, pmap);
		if (ret) {
			mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
			goto p_err;
		}

		return ret;
	}

p_err:
	return ret;
}

static const struct is_subdev_ops is_subdev_mcs_ops = {
#ifdef USE_VRA_OTF
	.bypass			= is_ischain_mcs_bypass,
#else
	.bypass			= NULL,
#endif
	.cfg			= NULL,
	.tag			= is_ischain_mcs_tag,
};

const struct is_subdev_ops *pablo_get_is_subdev_mcs_ops(void)
{
	return &is_subdev_mcs_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_is_subdev_mcs_ops);
