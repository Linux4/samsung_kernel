/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef PABLO_KUNIT_TEST_UTILS_H
#define PABLO_KUNIT_TEST_UTILS_H

#include "is-device-ischain.h"
#include "is-param.h"

void *pablo_kunit_get_param_from_frame(struct is_frame *frame, u32 index);
void *pablo_kunit_get_param_from_device(struct is_device_ischain *device, u32 index);
int pablo_kunit_compare_param(struct is_frame *frame, enum is_param param_idx, void *param_dst);

#endif /* PABLO_KUNIT_TEST_UTILS_H */
