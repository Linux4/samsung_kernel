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

static int is_ischain_lme_cfg(struct is_subdev *leader,
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
	struct param_lme_dma *dma_input;
	struct param_control *control;
	struct is_device_ischain *device;
	u32 hw_format = DMA_INPUT_FORMAT_Y;
	u32 hw_bitwidth = DMA_INPUT_BIT_WIDTH_8BIT;
	u32 hw_plane = DMA_INPUT_PLANE_1;
	u32 hw_order = DMA_INPUT_ORDER_NO;
	u32 width, height;

	device = (struct is_device_ischain *)device_data;

	FIMC_BUG(!leader);
	FIMC_BUG(!device);
	FIMC_BUG(!incrop);
	FIMC_BUG(!lindex);
	FIMC_BUG(!hindex);
	FIMC_BUG(!indexes);

	width = incrop->w;
	height = incrop->h;
	group = &device->group_lme;

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
		control = is_itf_g_param(device, NULL, PARAM_LME_CONTROL);
		if (test_bit(IS_GROUP_START, &group->state)) {
			control->cmd = CONTROL_COMMAND_START;
			control->bypass = CONTROL_BYPASS_DISABLE;
		} else {
			control->cmd = CONTROL_COMMAND_STOP;
			control->bypass = CONTROL_BYPASS_DISABLE;
		}
		*lindex |= LOWBIT_OF(PARAM_LME_CONTROL);
		*hindex |= HIGHBIT_OF(PARAM_LME_CONTROL);
		(*indexes)++;
	}

	dma_input = is_itf_g_param(device, frame, PARAM_LME_DMA);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state)) {
		if (test_bit(IS_GROUP_VOTF_INPUT, &group->state)) {
			dma_input->input_cmd = DMA_INPUT_COMMAND_ENABLE;
			dma_input->input_v_otf_enable = OTF_INPUT_COMMAND_ENABLE;
		} else {
			dma_input->input_cmd = DMA_INPUT_COMMAND_DISABLE;
			dma_input->input_v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
		}
	} else {
		dma_input->input_cmd = DMA_INPUT_COMMAND_ENABLE;
		dma_input->input_v_otf_enable = OTF_INPUT_COMMAND_DISABLE;
	}

	dma_input->input_format = hw_format;
	dma_input->input_bitwidth = hw_bitwidth;
	dma_input->input_order = change_to_input_order(hw_order);
	dma_input->input_msb = hw_bitwidth - 1;
	dma_input->input_plane = hw_plane;
	dma_input->input_width = width;
	dma_input->input_height = height;

	*lindex |= LOWBIT_OF(PARAM_LME_DMA);
	*hindex |= HIGHBIT_OF(PARAM_LME_DMA);
	(*indexes)++;

p_err:
	return ret;
}

static int is_ischain_lme_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret = 0;
	struct is_group *group;
	struct lme_param *lme_param;
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

	mdbgs_ischain(4, "LME TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = &device->group_lme;
	leader = subdev->leader;
	lindex = hindex = indexes = 0;
	lme_param = &device->is_region->parameter.lme;

	inparm.x = 0;
	inparm.y = 0;
	inparm.w = lme_param->dma.input_width;
	inparm.h = lme_param->dma.input_height;

	if (IS_NULL_CROP(incrop))
		*incrop = inparm;

	if (!COMPARE_CROP(incrop, &inparm) ||
		test_bit(IS_SUBDEV_FORCE_SET, &leader->state)) {
		ret = is_ischain_lme_cfg(subdev,
			device,
			frame,
			incrop,
			otcrop,
			&lindex,
			&hindex,
			&indexes);
		if (ret) {
			merr("is_ischain_lme_cfg is fail(%d)", device, ret);
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

const struct is_subdev_ops is_subdev_lme_ops = {
	.bypass			= NULL,
	.cfg			= is_ischain_lme_cfg,
	.tag			= is_ischain_lme_tag,
};
