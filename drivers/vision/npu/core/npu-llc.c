/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <soc/samsung/exynos-sci.h>
#include "npu-scheduler.h"
#include "npu-log.h"

void npu_set_llc(struct npu_scheduler_info *info)
{
	if (info->mode == NPU_PERF_MODE_NPU_BOOST) {
		if (!info->llc_status) {
			/* If llc disabled, set llc */
			llc_region_alloc(LLC_REGION_NPU, 1);
			info->llc_status = 1;
			npu_info("npu set llc(mode:%u, status:%u)\n",
					info->mode, info->llc_status);
		}
	} else {
		/* Non NPU_PERF_MODE_NPU_BOOST or
		 * NPU_PERF_MODE_NPU_SINGLE_BOOST.
		 */
		if (info->llc_status) {
			/* If llc enabled, put llc */
			llc_region_alloc(LLC_REGION_NPU, 0);
			info->llc_status = 0;
			npu_info("npu put llc(mode:%u, status:%u)\n",
					info->mode, info->llc_status);
		}
	}
	return;
}
