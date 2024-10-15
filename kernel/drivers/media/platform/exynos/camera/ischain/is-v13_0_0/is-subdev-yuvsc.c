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

#include "is-device-ischain.h"

static inline int _check_cropRegion(struct is_device_ischain *device, struct camera2_node *node)
{
	if (IS_NULL_CROP((struct is_crop *)node->input.cropRegion)) {
		mlverr("incrop is NULL", device, node->vid);
		return -EINVAL;
	}

	if (IS_NULL_CROP((struct is_crop *)node->output.cropRegion)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}

	return 0;
}

static int _yuvsc_otf_out_cfg(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	IS_DECLARE_PMAP(pmap))
{
	struct param_otf_output *otf_output;
	struct is_crop *otcrop;
	int ret;

	set_bit(PARAM_YUVSC_OTF_OUTPUT, pmap);

	otf_output = is_itf_g_param(device, ldr_frame, PARAM_YUVSC_OTF_OUTPUT);
	if (test_bit(IS_GROUP_OTF_OUTPUT, &group->state))
		otf_output->cmd = OTF_OUTPUT_COMMAND_ENABLE;
	else
		otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;

	if (!otf_output->cmd)
		return 0;

	ret = _check_cropRegion(device, node);
	if (ret)
		return ret;

	otcrop = (struct is_crop *)node->output.cropRegion;

	if ((otf_output->width != otcrop->w) || (otf_output->height != otcrop->h))
		mlvinfo("[F%d]OTF otcrop[%d, %d, %d, %d]\n", device,
			node->vid, ldr_frame->fcount,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	otf_output->width = otcrop->w;
	otf_output->height = otcrop->h;

	return 0;
}

static int _yuvsc_otf_in_cfg(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	IS_DECLARE_PMAP(pmap))
{
	struct param_otf_input *otf_input;
	struct is_crop *incrop;
	int ret;

	set_bit(PARAM_YUVSC_OTF_INPUT, pmap);

	otf_input = is_itf_g_param(device, ldr_frame, PARAM_YUVSC_OTF_INPUT);
	if (test_bit(IS_GROUP_OTF_INPUT, &group->state))
		otf_input->cmd = OTF_INPUT_COMMAND_ENABLE;
	else
		otf_input->cmd = OTF_INPUT_COMMAND_DISABLE;

	if (!otf_input->cmd)
		return 0;

	ret = _check_cropRegion(device, node);
	if (ret)
		return ret;

	incrop = (struct is_crop *)node->input.cropRegion;

	if ((otf_input->width != incrop->w) || (otf_input->height != incrop->h))
		mlvinfo("[F%d]OTF incrop[%d, %d, %d, %d]\n", device,
			node->vid, ldr_frame->fcount,
			incrop->x, incrop->y, incrop->w, incrop->h);

	otf_input->width = incrop->w;
	otf_input->height = incrop->h;

	return 0;
}

static void _yuvsc_control_cfg(struct is_device_ischain *device,
	struct is_group *group,
	struct is_frame *frame,
	IS_DECLARE_PMAP(pmap))
{
	struct param_control *control;

	set_bit(PARAM_YUVSC_CONTROL, pmap);

	control = is_itf_g_param(device, frame, PARAM_YUVSC_CONTROL);
	if (test_bit(IS_GROUP_STRGEN_INPUT, &group->state))
		control->strgen = CONTROL_COMMAND_START;
	else
		control->strgen = CONTROL_COMMAND_STOP;
}

static int is_ischain_yuvsc_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	struct is_device_ischain *device;
	struct camera2_node *out_node = NULL;
	struct is_crop *incrop, *otcrop;
	struct is_group *group;
	IS_DECLARE_PMAP(pmap);
	int ret;

	device = (struct is_device_ischain *)device_data;

	mdbgs_ischain(4, "YUVSC TAG\n", device);

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;

	group = container_of(subdev, struct is_group, leader);

	IS_INIT_PMAP(pmap);

	_yuvsc_control_cfg(device, group, frame, pmap);

	out_node = &frame->shot_ext->node_group.leader;

	ret = _yuvsc_otf_in_cfg(device, group, frame, out_node, pmap);
	if (ret) {
		mlverr("[F%d] otf_in_cfg fail. ret %d", device, node->vid,
				frame->fcount, ret);
		return ret;
	}

	ret = _yuvsc_otf_out_cfg(device, group, frame, out_node, pmap);
	if (ret) {
		mlverr("[F%d] otf_out_cfg fail. ret %d", device, node->vid,
				frame->fcount, ret);
		return ret;
	}

	out_node->result = 1;

	ret = is_itf_s_param(device, frame, pmap);
	if (ret) {
		mlverr("[F%d] is_itf_s_param is fail(%d)", device, node->vid, frame->fcount, ret);
		return ret;
	}

	return 0;
}

static const struct is_subdev_ops is_subdev_yuvsc_ops = {
	.tag	= is_ischain_yuvsc_tag,
};

const struct is_subdev_ops *pablo_get_is_subdev_yuvsc_ops(void)
{
	return &is_subdev_yuvsc_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_is_subdev_yuvsc_ops);
