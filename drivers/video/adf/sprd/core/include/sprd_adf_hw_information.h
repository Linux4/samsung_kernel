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

#ifndef _SPRD_ADF_HW_INFORMATION_H_
#define _SPRD_ADF_HW_INFORMATION_H_

#include "sprd_adf_common.h"

int sprd_adf_get_display_config(struct sprd_display_config *display_config);

void sprd_adf_destory_display_config(struct sprd_display_config *config);

struct sprd_adf_device_capability *sprd_adf_get_device_capability(unsigned
						int dev_id);
struct sprd_adf_interface_capability
	*sprd_adf_get_interface_capability(unsigned int dev_id,
		unsigned int intf_id);
struct sprd_adf_overlayengine_capability
	*sprd_adf_get_overlayengine_capability(unsigned int dev_id,
		unsigned int eng_id);

#endif
