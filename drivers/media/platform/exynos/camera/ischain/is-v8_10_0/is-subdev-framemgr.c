// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include "is-core.h"
#include "is-param.h"
#include "is-device-ischain.h"
#include "pablo-debug.h"
#include "pablo-mem.h"
#include "is-hw-common-dma.h"

int is_subdev_internal_get_buffer_size(const struct is_subdev *subdev,
	u32 *width, u32 *height, u32 *sbwc_en,
	u32 *comp_64b_align, u32 *lossy_byte32num,
	u32 *sbwc_block_width, u32 *sbwc_block_height)
{
	if (!width) {
		mserr("width is NULL", subdev, subdev);
		return -EINVAL;
	}
	if (!height) {
		mserr("height is NULL", subdev, subdev);
		return -EINVAL;
	}
	if (!sbwc_en) {
		mserr("sbwc_en is NULL", subdev, subdev);
		return -EINVAL;
	}

	switch (subdev->id) {
	default:
		*width = subdev->output.width;
		*height = subdev->output.height;
		*sbwc_en = NONE;
		break;
	}

	return 0;
}
