/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_IMGLOADER_H
#define PABLO_ICPU_IMGLOADER_H

struct pablo_icpu_imgloader_ops {
	int (*verify)(const void *kva, size_t size);
};

int pablo_icpu_imgloader_init(void *dev, bool signature, struct pablo_icpu_imgloader_ops *ops);
int pablo_icpu_imgloader_load(void *icpubuf);
void pablo_icpu_imgloader_release(void);

#endif

