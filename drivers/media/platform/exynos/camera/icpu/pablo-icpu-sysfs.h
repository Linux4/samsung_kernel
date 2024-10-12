/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_SYSFS_H
#define PABLO_ICPU_SYSFS_H

int pablo_icpu_sysfs_probe(struct device *dev);
void pablo_icpu_sysfs_remove(struct device *dev);

#endif
