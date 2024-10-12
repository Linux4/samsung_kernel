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

void is_subdev_internal_get_sbwc_type(const struct is_subdev *subdev,
					u32 *sbwc_type, u32 *lossy_byte32num)
{
	*lossy_byte32num = 0;

	switch (subdev->id) {
	case ENTRY_YPP:
		*sbwc_type = COMP;
		break;
	default:
		*sbwc_type = NONE;
		break;
	}
}

int is_subdev_internal_get_buffer_size(const struct is_subdev *subdev,
	u32 *width, u32 *height, u32 *sbwc_block_width, u32 *sbwc_block_height)
{
	struct is_core *core = is_get_is_core();
	u32 max_width, max_height;

	if (!width) {
		mserr("width is NULL", subdev, subdev);
		return -EINVAL;
	}
	if (!height) {
		mserr("height is NULL", subdev, subdev);
		return -EINVAL;
	}

	switch (subdev->id) {
	case ENTRY_YPP:
		if (subdev->use_shared_framemgr) {
			is_sensor_g_max_size(&max_width, &max_height);

			if (core->hardware.ypp_internal_buf_max_width)
				*width = MIN(max_width, core->hardware.ypp_internal_buf_max_width);
			else
				*width = MIN(max_width, subdev->leader->constraints_width);

			*height = max_height;

			if (core->vender.isp_max_width)
				*width = MIN(*width, core->vender.isp_max_width);
			if (core->vender.isp_max_height)
				*height = core->vender.isp_max_height;
		} else {
			*width = MIN(subdev->output.width, subdev->leader->constraints_width);
			*height = subdev->output.height;
		}
		if (sbwc_block_width)
			*sbwc_block_width = YPP_COMP_BLOCK_WIDTH;
		if (sbwc_block_height)
			*sbwc_block_height = YPP_COMP_BLOCK_HEIGHT;

		break;
	default:
		*width = subdev->output.width;
		*height = subdev->output.height;
		if (sbwc_block_width)
			*sbwc_block_width = 0;
		if (sbwc_block_height)
			*sbwc_block_height = 0;

		break;
	}

	return 0;
}
