/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
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

#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>

#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-param.h"
int is_pafs_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret = 0;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_PAF0 + index;
	video->group_ofs = offsetof(struct is_device_ischain, group_paf);
	video->subdev_id = ENTRY_PAF;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_PAFXS_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_PAFXS_NAME(index),
		video_num,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}
