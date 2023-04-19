// SPDX-License-Identifier: GPL-2.0
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

#include "pablo-kunit-test-utils.h"

void *pablo_kunit_get_param_from_frame(struct is_frame *frame, u32 index)
{
	ulong dst_base, dst_param;

	dst_base = (ulong)frame->parameter;
	dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));

	return (void *)dst_param;
}

void *pablo_kunit_get_param_from_device(struct is_device_ischain *device, u32 index)
{
	ulong dst_base, dst_param;

	dst_base = (ulong)&device->is_region->parameter;
	dst_param = (dst_base + (index * PARAMETER_MAX_SIZE));

	return (void *)dst_param;
}

int pablo_kunit_compare_param(struct is_frame *frame, enum is_param param_idx, void *param_dst)
{
	int i;
	void *param_src = pablo_kunit_get_param_from_frame(frame, param_idx);
	u32 *src = (u32 *)param_src;
	u32 *dst = (u32 *)param_dst;

	for (i = 0; i < PARAMETER_MAX_MEMBER; i++)
		pr_info("PARAM[%d]: [%d] src(%d) dst(%d)\n", param_idx, i, src[i], dst[i]);

	return memcmp(param_dst, param_src, PARAMETER_MAX_SIZE);
}
