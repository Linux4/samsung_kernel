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

#ifndef PABLO_ICPU_H
#define PABLO_ICPU_H

#include "pablo-icpu-log.h"

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
#define KUNIT_EXPORT_SYMBOL(x) EXPORT_SYMBOL_GPL(x)
#else
#define KUNIT_EXPORT_SYMBOL(x)
#endif

#endif
