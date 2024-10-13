/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SPRD_ADF_DEVICE_H_
#define _SPRD_ADF_DEVICE_H_

#include "sprd_adf_common.h"

#define SPRD_ADF_MAX_DEVICE 1

struct sprd_adf_device *sprd_adf_create_devices(
			struct platform_device *pdev,
			size_t n_devs);

int sprd_adf_destory_devices(
			struct sprd_adf_device *devs,
			size_t n_devs);
#endif
