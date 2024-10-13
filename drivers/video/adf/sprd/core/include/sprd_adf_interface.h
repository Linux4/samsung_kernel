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

#ifndef _SPRD_ADF_INTERFACE_H_
#define _SPRD_ADF_INTERFACE_H_

#include "sprd_adf_common.h"

#define SPRD_ADF_MAX_INTERFACE 2

struct interfaces_info {
	enum adf_interface_type type;
	u32 flags;
	char *name;
};

struct sprd_adf_interface *sprd_adf_create_interfaces(
			struct platform_device *pdev,
			struct sprd_adf_device *dev,
			size_t n_intfs);

int sprd_adf_destory_interfaces(
			struct sprd_adf_interface *intfs,
			size_t n_intfs);
#endif
