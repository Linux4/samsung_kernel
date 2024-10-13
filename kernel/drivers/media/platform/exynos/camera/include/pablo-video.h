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

#ifndef PABLO_VIDEO_H
#define PABLO_VIDEO_H

int is_video_probe(struct is_video *video,
	const char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct v4l2_device *v4l2_dev,
	const struct v4l2_ioctl_ops *ioctl_ops);

struct platform_driver *pablo_video_get_platform_driver(void);

#endif /* PABLO_VIDEO_H */
