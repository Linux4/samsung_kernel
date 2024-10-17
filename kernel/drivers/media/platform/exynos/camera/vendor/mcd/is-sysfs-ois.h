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

#ifndef _IS_SYSFS_OIS_H_
#define _IS_SYSFS_OIS_H_

bool read_ois_version(void);
void ois_factory_resource_clean(void);

void is_create_ois_sysfs(struct class *camera_class);
void is_destroy_ois_sysfs(struct class *camera_class);
#endif /* _IS_SYSFS_OIS_H_ */
