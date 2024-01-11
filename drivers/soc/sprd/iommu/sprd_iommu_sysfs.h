/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
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

#ifndef __SPRD_IOMMU_SYSFS_H__
#define __SPRD_IOMMU_SYSFS_H__

#include <linux/device.h>

int sprd_iommu_sysfs_create(struct sprd_iommu_dev *device,
							const char *dev_name);

int sprd_iommu_sysfs_destroy(struct sprd_iommu_dev *device,
							const char *dev_name);

#endif
