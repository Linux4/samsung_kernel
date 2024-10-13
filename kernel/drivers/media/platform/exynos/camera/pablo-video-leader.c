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

#include "is-groupmgr.h"
#include "pablo-device.h"
#include "pablo-debug.h"

static int pablo_leader_buf_queue(struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct pablo_device *pd = (struct pablo_device *)ivc->device;
	int ret;

	ret = CALL_GROUP_OPS(ivc->group, buffer_queue, ivc->queue, vb->index);
	if (ret)
		mgerr("group buffer_queue failed: %d", pd, ivc->group, ret);

	return ret;
}

static int pablo_leader_buf_finish(struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct pablo_device *pd = (struct pablo_device *)ivc->device;
	int ret;

	ret = CALL_GROUP_OPS(ivc->group, buffer_finish, vb->index);
	if (ret)
		mgerr("group buffer_finish failed: %d", pd, ivc->group, ret);

	return ret;
}

const struct pablo_video_ops pablo_leader_vops = {
	.buf_queue = pablo_leader_buf_queue,
	.buf_finish = pablo_leader_buf_finish,
};

const struct pablo_video_ops *pablo_get_pv_ops_leader(void)
{
	return &pablo_leader_vops;
}
