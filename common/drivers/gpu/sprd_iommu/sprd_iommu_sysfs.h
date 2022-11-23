#ifndef __SPRD_IOMMU_SYSFS_H__
#define __SPRD_IOMMU_SYSFS_H__

#include <linux/device.h>

int sprd_iommu_sysfs_register(struct sprd_iommu_dev *device, const char *dev_name);

int sprd_iommu_sysfs_unregister(struct sprd_iommu_dev *device, const char *dev_name);

#endif
