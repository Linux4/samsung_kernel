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

int is_lmec_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_LME0 + index;
	video->group_ofs = offsetof(struct is_device_ischain, group_lme);
	video->subdev_id = ENTRY_LMEC;
	video->subdev_ofs = offsetof(struct is_device_ischain, lmec);
	video->buf_rdycount = VIDEO_LMEXC_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_LMEXC_NAME(index),
		video_num,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}
int is_lmes_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_LME0 + index;
	video->group_ofs = offsetof(struct is_device_ischain, group_lme);
	video->subdev_id = ENTRY_LMES;
	video->subdev_ofs = offsetof(struct is_device_ischain, lmes);
	video->buf_rdycount = VIDEO_LMEXS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_LMEXS_NAME(index),
		video_num,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}
