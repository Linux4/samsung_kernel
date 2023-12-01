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

#ifndef PABLO_ICPU_ENUM_H
#define PABLO_ICPU_ENUM_H

enum {
	PABLO_ICPU_PRELOAD_FLAG_LOAD = 0,
	PABLO_ICPU_PRELOAD_FLAG_FORCE_RELEASE = 1,
	PABLO_ICPU_PRELOAD_FLAG_INVALID,
};

#endif
