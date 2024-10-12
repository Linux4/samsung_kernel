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

#define SENSOR_2EXP_MODE_SHORT_VC_MASK	0x1

static inline bool is_sensor_sub_vc_skip(struct is_device_sensor *sensor,
		struct is_subdev *subdev, int vc)
{
	ulong seamless_mode = sensor->seamless_state & IS_SENSOR_SEAMLESS_MODE_MASK;

	/* Skip VC tagging is only valid for VC1/3 */
	if (!(vc & SENSOR_2EXP_MODE_SHORT_VC_MASK))
		return false;
	else if (test_bit(IS_SENSOR_SINGLE_MODE, &seamless_mode) &&
			!test_bit(IS_SENSOR_SWITCHING, &seamless_mode))
		return true;
	else if (test_bit(IS_SENSOR_2EXP_MODE, &seamless_mode) &&
			test_bit(IS_SENSOR_SWITCHING, &seamless_mode))
		return true;

	return false;
}

static int __sensor_dma_out_cfg(struct is_device_sensor *device,
	struct is_frame *frame,
	struct camera2_node *node,
	u32 async)
{
	struct is_fmt *fmt;
	struct sensor_dma_output *dma_output;
	struct is_crop *otcrop;
	struct is_crop otcrop_cfg;
	u32 hw_sbwc = DMA_INPUT_SBWC_DISABLE;
	u32 flag_extra;
	u32 vc = GET_LVN_VC_OFFSET(node->vid);

	dma_output = &device->dma_output[vc];

	if (dma_output->cmd != node->request)
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->cmd, node->request);
	if (!node->request) {
		dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;
		if (!async)
			return 0;
	} else {
		dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	}

	otcrop = (struct is_crop *)node->output.cropRegion;
	if (IS_NULL_CROP(otcrop)) {
		mlverr("otcrop is NULL", device, node->vid);
		return -EINVAL;
	}
	otcrop_cfg = *otcrop;

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		merr("pixelformat(%c%c%c%c) is not found", device,
			(char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}

	memcpy(&dma_output->fmt, fmt, sizeof(struct is_fmt));
	flag_extra = (node->flags & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;
	if (flag_extra)
		hw_sbwc = (SBWC_BASE_ALIGN_MASK | flag_extra);

	if (dma_output->sbwc_type != hw_sbwc)
		mlvinfo("[F%d]WDMA sbwc_type: %d -> %d\n", device,
			node->vid, frame->fcount,
			dma_output->sbwc_type, hw_sbwc);
	dma_output->sbwc_type = hw_sbwc;
	dma_output->width = otcrop_cfg.w;
	dma_output->height = otcrop_cfg.h;

	mdbgs_sensor(2, "[F%d] , width: %d, height: %d, sbwc : %d, cmd : %d\n", device,
		frame->fcount, dma_output->width, dma_output->height, dma_output->sbwc_type, dma_output->cmd);

	return 0;
}

static int is_sensor_subdev_tag(struct is_subdev *subdev,
	void *device_data,
	struct is_frame *frame,
	struct camera2_node *node)
{
	int ret;
	struct is_device_sensor *device;
	struct camera2_node *cap_node;
	struct is_sub_frame *sframe;
	struct v4l2_subdev *subdev_csi;
	struct is_group *group;
	dma_addr_t *target_addr;
	u32 vc;
	u32 async;
	int i, j;

	device = (struct is_device_sensor *)device_data;
	subdev_csi = device->subdev_csi;
	group = &device->group_sensor;

	if (!frame) {
		merr("[SEN] process is empty", device);
		return -EINVAL;
	}

	if (!IS_ENABLED(USE_SENSOR_GROUP_LVN))
		return 0;

	async = !atomic_read(&group->scount);
	for (i = 0; i < CAPTURE_NODE_MAX; i++) {
		cap_node = &frame->shot_ext->node_group.capture[i];

		if(!IS_SENSOR_LVN_VID(cap_node->vid))
			continue;

		ret = __sensor_dma_out_cfg(device, frame, cap_node, async);
		if (ret) {
			mlverr("[F%d] dma_out_cfg error\n", device, cap_node->vid,
					frame->fcount);
			return ret;
		}

		sframe = &frame->cap_node.sframe[i];
		if (!sframe->id || !IS_SENSOR_LVN_VID(sframe->id))
			continue;

		target_addr = NULL;
		ret = is_sensor_get_capture_slot(frame, &target_addr, sframe->id);
		if (ret) {
			mrerr("Invalid capture slot(%d)", device, frame, sframe->id);
			return ret;
		}

		if (target_addr) {
			for (j = 0; j < sframe->num_planes; j++)
				target_addr[j] = sframe->dva[j];
		}

		vc = GET_LVN_VC_OFFSET(cap_node->vid);

		/* When it's on 2EXP single mode, only VC0/2 could be enabled. */
		if (is_sensor_sub_vc_skip(device, subdev, vc))
			cap_node->result = 0;
		else
			ret = is_sensor_sbuf_tag(device, subdev, subdev_csi, frame, vc);

		if (ret) {
			mswarn("%d frame is drop", device, subdev, frame->fcount);
			cap_node->request = 0;
		}
	}

	return 0;
}

const struct is_subdev_ops is_subdev_sensor_ops = {
	.bypass	= NULL,
	.cfg	= NULL,
	.tag	= is_sensor_subdev_tag,
};
