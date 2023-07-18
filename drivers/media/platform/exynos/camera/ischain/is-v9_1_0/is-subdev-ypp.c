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
#include "is-device-sensor.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

static int is_ischain_ypp_stripe_cfg(struct is_subdev *subdev,
		struct is_frame *frame,
		struct is_crop *incrop,
		struct is_crop *otcrop,
		u32 bitwidth)
{
	struct is_group *group;
	u32 stripe_w, dma_offset = 0;
	u32 region_id = frame->stripe_info.region_id;
	struct camera2_stream *stream = (struct camera2_stream *) frame->shot_ext;
	group = frame->group;
	/* Input crop configuration & RDMA offset configuration*/
	if (!region_id) {
		/* Left region */
		/* Stripe width should be 4 align because of 4 ppc */
		if (stream->stripe_h_pix_nums[region_id]) {
			stripe_w = stream->stripe_h_pix_nums[region_id];
		}
		else {
			stripe_w = ALIGN(incrop->w / frame->stripe_info.region_num, 4);
			/* For SBWC Lossless, width should be 512 align with margin */
			stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w, STRIPE_WIDTH_ALIGN / 2);
		}
		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", subdev, subdev, frame,
									frame->stripe_info.region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}
		frame->stripe_info.in.h_pix_num = stripe_w;
		frame->stripe_info.region_base_addr[0] = frame->dvaddr_buffer[0];
		frame->stripe_info.region_base_addr[1] = frame->dvaddr_buffer[1];
	} else if (region_id < frame->stripe_info.region_num - 1) {
		/* Middle region */
		if (stream->stripe_h_pix_nums[region_id])
			stripe_w = stream->stripe_h_pix_nums[region_id] - frame->stripe_info.in.h_pix_num;
		else {
			stripe_w = ALIGN((incrop->w * (frame->stripe_info.region_id + 1) / frame->stripe_info.region_num) - frame->stripe_info.in.h_pix_num, 4);
			stripe_w = ALIGN_UPDOWN_STRIPE_WIDTH(stripe_w, STRIPE_WIDTH_ALIGN);
		}
		if (stripe_w == 0) {
			msrdbgs(3, "Skip current stripe[#%d] region because stripe_width is too small(%d)\n", subdev, subdev, frame,
									frame->stripe_info.region_id, stripe_w);
			frame->stripe_info.region_id++;
			return -EAGAIN;
		}

		stripe_w += STRIPE_MARGIN_WIDTH;
		/* Consider RDMA offset. */
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (stream->stripe_h_pix_nums[region_id]) {
				dma_offset = frame->stripe_info.in.h_pix_num;
				dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
				dma_offset = ((dma_offset * bitwidth * incrop->h) / BITS_PER_BYTE);
			} else {
				dma_offset = frame->stripe_info.in.h_pix_num - STRIPE_MARGIN_WIDTH;
				dma_offset *= bitwidth / BITS_PER_BYTE;
			}
		}
	} else {
		/* Right region */
		stripe_w = incrop->w - frame->stripe_info.in.h_pix_num;

		/* Consider RDMA offset. */
		if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
			if (stream->stripe_h_pix_nums[region_id]) {
				dma_offset = frame->stripe_info.in.h_pix_num;
				dma_offset += STRIPE_MARGIN_WIDTH * ((2 * (region_id - 1)) + 1);
				dma_offset = ((dma_offset * bitwidth * incrop->h) / BITS_PER_BYTE);
			} else {
				dma_offset = frame->stripe_info.in.h_pix_num - STRIPE_MARGIN_WIDTH;
				dma_offset *= bitwidth / BITS_PER_BYTE;
			}
		}
	}
	/* Add stripe processing horizontal margin into each region. */
	stripe_w += STRIPE_MARGIN_WIDTH;

	incrop->w = stripe_w;

	/**
	 * Output crop configuration.
	 * No crop & scale.
	 */
	otcrop->x = 0;
	otcrop->y = 0;
	otcrop->w = incrop->w;
	otcrop->h = incrop->h;

	frame->dvaddr_buffer[0] = frame->stripe_info.region_base_addr[0] + dma_offset;
	frame->dvaddr_buffer[1] = frame->stripe_info.region_base_addr[1] + dma_offset;

	msrdbgs(3, "stripe_in_crop[%d][%d, %d, %d, %d] offset %x\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			incrop->x, incrop->y, incrop->w, incrop->h, dma_offset);
	msrdbgs(3, "stripe_ot_crop[%d][%d, %d, %d, %d]\n", subdev, subdev, frame,
			frame->stripe_info.region_id,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);
	return 0;
}

static int is_ischain_ypp_cfg(struct is_subdev *leader,
	void *device_data,
	struct is_frame *frame,
	struct is_crop *incrop,
	struct is_crop *otcrop,
	u32 *lindex,
	u32 *hindex,
	u32 *indexes)
{
	int ret = 0;
	struct is_group *group;
	struct is_queue *queue;
	struct param_dma_input *dma_input;
	struct param_otf_output *otf_output;
	struct param_stripe_input *stripe_input;
	struct param_control *control;
	struct is_device_ischain *device;
	struct is_device_sensor *sensor;
	u32 hw_format = DMA_INPUT_FORMAT_YUV422;
	u32 hw_bitwidth = DMA_INPUT_BIT_WIDTH_10BIT;
	u32 hw_order = DMA_INPUT_ORDER_YCbYCr;
	u32 hw_plane = 2;
	struct is_crop incrop_cfg, otcrop_cfg;
	int stripe_ret = -1;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!device->sensor);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	sensor = device->sensor;
	group = &device->group_ypp;
	incrop_cfg = *incrop;
	otcrop_cfg = *otcrop;

	queue = GET_SUBDEV_QUEUE(leader);
	if (!queue) {
		merr("queue is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (!queue->framecfg.format) {
			merr("format is NULL", device);
			ret = -EINVAL;
			goto p_err;
		}

		hw_format = queue->framecfg.format->hw_format;
		hw_bitwidth = queue->framecfg.format->hw_bitwidth; /* memory width per pixel */
		hw_order = queue->framecfg.format->hw_order;
		hw_plane = queue->framecfg.format->hw_plane;
	}

	/* Configure Conrtol */
	if (!frame) {
		control = is_itf_g_param(device, NULL, PARAM_YPP_CONTROL);
		if (test_bit(IS_GROUP_START, &group->state)) {
			control->cmd = CONTROL_COMMAND_START;
			control->bypass = CONTROL_BYPASS_DISABLE;
		} else {
			control->cmd = CONTROL_COMMAND_STOP;
			control->bypass = CONTROL_BYPASS_DISABLE;
		}
		*lindex |= LOWBIT_OF(PARAM_YPP_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_YPP_CONTROL);
		(*indexes)++;
	}

	if (IS_ENABLED(CHAIN_USE_STRIPE_PROCESSING) && frame && frame->stripe_info.region_num) {
		while(stripe_ret)
			stripe_ret =is_ischain_ypp_stripe_cfg(leader, frame,
				&incrop_cfg, &otcrop_cfg,
				hw_bitwidth);
	}

	dma_input = is_itf_g_param(device, frame, PARAM_YPP_DMA_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
			dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_ENABLE;
		} else {
			dma_input->cmd = DMA_INPUT_COMMAND_DISABLE;
			dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
		}
	} else {
		dma_input->cmd = DMA_INPUT_COMMAND_ENABLE;
		dma_input->v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	}

	dma_input->format = hw_format;
	dma_input->bitwidth = hw_bitwidth;
	dma_input->msb = sensor->image.format.hw_bitwidth - 1;/* msb zero padding by HW constraint */
	dma_input->order = change_to_input_order(hw_order);
	dma_input->plane = hw_plane;
	dma_input->width = incrop_cfg.w;
	dma_input->height = incrop_cfg.h;

	dma_input->dma_crop_offset = (incrop_cfg.x << 16) | (incrop_cfg.y << 0);
	dma_input->dma_crop_width = incrop_cfg.w;
	dma_input->dma_crop_height = incrop_cfg.h;
	dma_input->bayer_crop_offset_x = incrop_cfg.x;
	dma_input->bayer_crop_offset_y = incrop_cfg.y;
	dma_input->bayer_crop_width = incrop_cfg.w;
	dma_input->bayer_crop_height = incrop_cfg.h;
	dma_input->stride_plane0 = incrop->w;
	*lindex |= LOWBIT_OF(PARAM_YPP_DMA_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_YPP_DMA_INPUT);
	(*indexes)++;

	otf_output = is_itf_g_param(device, frame, PARAM_YPP_OTF_OUTPUT);
	otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	otf_output->width = incrop_cfg.w;
	otf_output->height = incrop_cfg.h;
	otf_output->format = OTF_OUTPUT_FORMAT_YUV422;
	otf_output->bitwidth = OTF_OUTPUT_BIT_WIDTH_10BIT;
	otf_output->order = OTF_INPUT_ORDER_BAYER_GR_BG;
	*lindex |= LOWBIT_OF(PARAM_YPP_OTF_OUTPUT);
	*hindex |= HIGHBIT_OF(PARAM_YPP_OTF_OUTPUT);
	(*indexes)++;

	stripe_input = is_itf_g_param(device, frame, PARAM_YPP_STRIPE_INPUT);
	if (frame && frame->stripe_info.region_num) {
		stripe_input->index = frame->stripe_info.region_id;
		stripe_input->total_count = frame->stripe_info.region_num;
		if (!frame->stripe_info.region_id) {
			stripe_input->left_margin = 0;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
		} else if (frame->stripe_info.region_id < frame->stripe_info.region_num - 1) {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = STRIPE_MARGIN_WIDTH;
		} else {
			stripe_input->left_margin = STRIPE_MARGIN_WIDTH;
			stripe_input->right_margin = 0;
		}
		stripe_input->full_width = leader->input.width;
		stripe_input->full_height = leader->input.height;
	} else {
		stripe_input->index = 0;
		stripe_input->total_count = 0;
		stripe_input->left_margin = 0;
		stripe_input->right_margin = 0;
		stripe_input->full_width = 0;
		stripe_input->full_height = 0;
	}
	*lindex |= LOWBIT_OF(PARAM_YPP_STRIPE_INPUT);
	*hindex |= HIGHBIT_OF(PARAM_YPP_STRIPE_INPUT);
	(*indexes)++;

p_err:
	return ret;
}

static int is_ischain_ypp_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct ypp_param *ypp_param;
	struct is_crop inparm;
	struct is_crop *incrop, *otcrop;
	struct is_subdev *leader;
	struct is_device_ischain *device;
	u32 lindex, hindex, indexes;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!subdev);
	FIMC_BUG(!device);
	FIMC_BUG(!device->is_region);
	FIMC_BUG(!frame);

	mdbgs_ischain(4, "YPP TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_ypp;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	ypp_param = &device->is_region->parameter.ypp;

	inparm.x = 0;
	inparm.y = 0;
	inparm.w = ypp_param->dma_input.width;
	inparm.h = ypp_param->dma_input.height;

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	/* not supported DMA input crop */
	if ((incrop->x != 0) || (incrop->y != 0))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		CHECK_STRIPE_CFG(&frame->stripe_info) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_ypp_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("is_ischain_ypp_cfg is fail(%d)", device, ret);
			goto p_err;
		}
		if (!COMPARE_CROP(incrop, &subdev->input.crop) ||
			debug_stream) {
			msrinfo("in_crop[%d, %d, %d, %d]\n", device, subdev, frame,
				incrop->x, incrop->y, incrop->w, incrop->h);

			subdev->input.crop = *incrop;
		}
	}

	ret = is_itf_s_param(device, frame, lindex, hindex, indexes);
	if (ret) {
		mrerr("is_itf_s_param is fail(%d)", device, frame, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct is_subdev_ops is_subdev_ypp_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_ypp_cfg,
	.tag			= is_ischain_ypp_tag,
};
