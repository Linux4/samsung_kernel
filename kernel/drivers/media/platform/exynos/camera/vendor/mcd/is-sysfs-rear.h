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

#ifndef _IS_SYSFS_REAR_H_
#define _IS_SYSFS_REAR_H_

void is_sysfs_hw_init(void);

int is_create_rear_sysfs(struct class *camera_class);
void is_destroy_rear_sysfs(struct class *camera_class);
#endif /* _IS_SYSFS_REAR_H_ */
