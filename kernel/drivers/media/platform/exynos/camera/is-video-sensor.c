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

#include "is-device-sensor.h"
#include "is-video.h"
#include "pablo-video.h"
#include "pablo-v4l2.h"

int is_ssx_video_probe(void *data)
{
	int ret = 0;
	struct is_device_sensor *device;
	struct is_video *video;
	char name[30];
	u32 device_id;

	FIMC_BUG(!data);

	device = (struct is_device_sensor *)data;
	device_id = device->device_id;
	video = &device->video;
	snprintf(name, sizeof(name), "%s%d", IS_VIDEO_SSX_NAME, device_id);

	if (!device->pdev) {
		err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	video->group_id = GROUP_ID_SS0 + device_id;
	video->subdev_ofs = offsetof(struct is_group, leader);

	ret = is_video_probe(video, name, IS_VIDEO_SS0_NUM + device_id, VFL_DIR_M2M,
			     &device->v4l2_dev, pablo_get_v4l2_ioctl_ops_sensor());
	if (ret)
		dev_err(&device->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}
