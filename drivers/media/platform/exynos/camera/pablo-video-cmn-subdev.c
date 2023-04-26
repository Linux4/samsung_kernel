// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-core.h"
#include "is-video.h"

int is_common_video_probe(void *data)
{
	int ret;
	struct is_core *core;
	struct is_video *video;

	core = (struct is_core *)data;
	if (!core) {
		probe_err("core is NULL");
		return -EINVAL;
	}

	if (!core->pdev) {
		probe_err("pdev is NULL");
		return -EINVAL;
	}

	video = &core->video_cmn;
	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_MAX;
	video->group_ofs = 0;
	video->subdev_id = ENTRY_INTERNAL;
	video->subdev_ofs = 0;
	video->buf_rdycount = 0;

	ret = is_video_probe(video,
		"common",
		IS_VIDEO_COMMON,
		VFL_DIR_RX,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL, NULL, NULL);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}
KUNIT_EXPORT_SYMBOL(is_common_video_probe);
