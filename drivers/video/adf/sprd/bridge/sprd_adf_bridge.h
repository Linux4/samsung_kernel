
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

#ifndef _SPRD_ADF_BRIDGE_H_
#define _SPRD_ADF_BRIDGE_H_

#include "sprd_adf_common.h"

extern struct sprd_display_config_entry shark_display_config_entry;
struct sprd_display_config_entry *sprd_adf_get_config_entry(void);

struct sprd_adf_device_ops *sprd_adf_get_device_ops(size_t index);
struct sprd_adf_interface_ops *sprd_adf_get_interface_ops(size_t index);

void *sprd_adf_get_device_private_data(struct platform_device *pdev,
			size_t index);
void sprd_adf_destory_device_private_data(void *data);

void *sprd_adf_get_interface_private_data(struct platform_device *pdev,
			size_t index);
void sprd_adf_destory_interface_private_data(void *data);

#endif
