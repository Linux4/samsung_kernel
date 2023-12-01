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
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"

int is_3ac_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_3AA0 + index;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AC;
	video->subdev_ofs = offsetof(struct is_device_ischain, txc);
	video->buf_rdycount = VIDEO_3XC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XC_NAME(index),
		video_num,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}

int is_3af_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_3AA0 + index;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AF;
	video->subdev_ofs = offsetof(struct is_device_ischain, txf);
	video->buf_rdycount = VIDEO_3XF_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XF_NAME(index),
		video_num,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}

int is_3ag_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_3AA0 + index;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AG;
	video->subdev_ofs = offsetof(struct is_device_ischain, txg);
	video->buf_rdycount = VIDEO_3XG_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XG_NAME(index),
		video_num,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}

int is_3al_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_3AA0 + index;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AL;
	video->subdev_ofs = offsetof(struct is_device_ischain, txl);
	video->buf_rdycount = VIDEO_3XL_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XL_NAME(index),
		video_num,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}

int is_3ao_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_3AA0 + index;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AO;
	video->subdev_ofs = offsetof(struct is_device_ischain, txo);
	video->buf_rdycount = VIDEO_3XO_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XO_NAME(index),
		video_num,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}

int is_3ap_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_3AA0 + index;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_3AP;
	video->subdev_ofs = offsetof(struct is_device_ischain, txp);
	video->buf_rdycount = VIDEO_3XP_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_3XP_NAME(index),
		video_num,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}

static int is_orbxc_queue_setup(struct vb2_queue *vq,
		unsigned *nbuffers, unsigned *nplanes,
		unsigned sizes[], struct device *alloc_devs[])
{
	struct is_video_ctx *ivc = vq->drv_priv;
	struct is_queue *iq = &ivc->queue;

	set_bit(IS_QUEUE_NEED_TO_KMAP, &iq->state);

	return is_queue_setup(vq, nbuffers, nplanes, sizes, alloc_devs);
}

const struct vb2_ops is_orbxc_vb2_ops = {
	.queue_setup		= is_orbxc_queue_setup,
	.wait_prepare		= is_wait_prepare,
	.wait_finish		= is_wait_finish,
	.buf_init		= is_buf_init,
	.buf_prepare		= is_buf_prepare,
	.buf_finish		= is_buf_finish,
	.buf_cleanup		= is_buf_cleanup,
	.start_streaming	= is_start_streaming,
	.stop_streaming		= is_stop_streaming,
	.buf_queue		= is_buf_queue,
};

int is_orbc_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_3AA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_3aa);
	video->subdev_id = ENTRY_ORBXC;
	video->subdev_ofs = offsetof(struct is_device_ischain, orbxc);
	video->buf_rdycount = VIDEO_ORBXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_ORBXC_NAME(index),
		video_num,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		NULL,
		&is_orbxc_vb2_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}
