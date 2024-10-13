/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IS_SYSFS_HWPARAM_H_
#define _IS_SYSFS_HWPARAM_H_

int is_add_rear_hwparam_sysfs(struct device *camera_dev);
int is_add_front_hwparam_sysfs(struct device *camera_dev);
#endif /* _IS_SYSFS_HWPARAM_H_ */
