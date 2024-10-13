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

#ifndef PABLO_V4L2_H
#define PABLO_V4L2_H

const struct v4l2_file_operations *pablo_get_v4l2_file_ops_default(void);
const struct v4l2_ioctl_ops *pablo_get_v4l2_ioctl_ops_default(void);
const struct v4l2_ioctl_ops *pablo_get_v4l2_ioctl_ops_sensor(void);
const struct vb2_ops *pablo_get_vb2_ops_default(void);

#endif /* PABLO_V4L2_H */
