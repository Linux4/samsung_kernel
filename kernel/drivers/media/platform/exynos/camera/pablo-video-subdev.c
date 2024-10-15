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

#include "is-subdev-ctrl.h"
#include "pablo-device.h"
#include "pablo-debug.h"

static int pablo_subdev_buf_finish(struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct pablo_device *pd = (struct pablo_device *)ivc->device;
	struct is_subdev *sd = ivc->subdev;
	unsigned int index = vb->index;
	struct is_framemgr *framemgr = GET_SUBDEV_FRAMEMGR(sd);
	struct is_frame *frame;

	if (unlikely(!test_bit(IS_SUBDEV_OPEN, &sd->state))) {
		warn("subdev is not opened, state: 0x%08lx", sd->state);
		return -EINVAL;
	}

	if (index >= framemgr->num_frames) {
		mserr("out of range (%d >= %d)", pd, sd, index, framemgr->num_frames);
		return -EINVAL;
	}
	frame = &framemgr->frames[index];

	framemgr_e_barrier_irq(framemgr, FMGR_IDX_18);

	if (frame->state == FS_COMPLETE) {
		trans_frame(framemgr, frame, FS_FREE);
	} else {
		merr("frame is empty from complete", pd);
		merr("frame(%d) is not com state(%d)", pd,
					index, frame->state);
		frame_manager_print_queues(framemgr);

		framemgr_x_barrier_irq(framemgr, FMGR_IDX_18);

		return -EINVAL;
	}

	framemgr_x_barrier_irq(framemgr, FMGR_IDX_18);

	return 0;
}

static int pablo_subdev_buf_queue(struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct pablo_device *pd = (struct pablo_device *)ivc->device;
	struct is_subdev *sd = ivc->subdev;
	unsigned int index = vb->index;
	struct is_framemgr *framemgr = GET_SUBDEV_FRAMEMGR(sd);
	struct is_frame *frame;
	unsigned long flags;

	if (index >= framemgr->num_frames) {
		mserr("out of range (%d >= %d)", pd, sd, index, framemgr->num_frames);
		return -EINVAL;
	}
	frame = &framemgr->frames[index];

	/* 1. check frame validation */
	if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
		mserr("frame %d is not initialized", sd, sd, index);
		return -EINVAL;
	}

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

	if (frame->state == FS_FREE) {
		trans_frame(framemgr, frame, FS_REQUEST);
	} else {
		mserr("frame %d is invalid state(%d)\n", sd, sd, index, frame->state);

		frame_manager_print_queues(framemgr);

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);

		return -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);

	return 0;
}

const struct pablo_video_ops pablo_video_ops_subdev = {
	.buf_queue = pablo_subdev_buf_queue,
	.buf_finish = pablo_subdev_buf_finish,
};

const struct pablo_video_ops *pablo_get_pv_ops_subdev(void)
{
	return &pablo_video_ops_subdev;
}
