/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Unisoc QOGIRN6PRO VPU driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#ifndef _SPRD_VPU_H
#define _SPRD_VPU_H

#include <linux/ioctl.h>
#include <linux/compat.h>


#define SPRD_VPU_IOCTL_MAGIC 'm'
#define VPU_CONFIG_FREQ _IOW(SPRD_VPU_IOCTL_MAGIC, 1, unsigned int)
#define VPU_GET_FREQ    _IOR(SPRD_VPU_IOCTL_MAGIC, 2, unsigned int)
#define VPU_ENABLE      _IO(SPRD_VPU_IOCTL_MAGIC, 3)
#define VPU_DISABLE     _IO(SPRD_VPU_IOCTL_MAGIC, 4)
#define VPU_ACQUAIRE    _IO(SPRD_VPU_IOCTL_MAGIC, 5)
#define VPU_RELEASE     _IO(SPRD_VPU_IOCTL_MAGIC, 6)
#define VPU_COMPLETE    _IO(SPRD_VPU_IOCTL_MAGIC, 7)
#define VPU_RESET       _IO(SPRD_VPU_IOCTL_MAGIC, 8)
#define VPU_HW_INFO     _IO(SPRD_VPU_IOCTL_MAGIC, 9)
#define VPU_VERSION     _IO(SPRD_VPU_IOCTL_MAGIC, 10)
#define VPU_GET_IOVA    _IOWR(SPRD_VPU_IOCTL_MAGIC, 11, struct iommu_map_data)
#define VPU_FREE_IOVA   _IOW(SPRD_VPU_IOCTL_MAGIC, 12, struct iommu_map_data)
#define VPU_GET_IOMMU_STATUS _IO(SPRD_VPU_IOCTL_MAGIC, 13)
#define VPU_SET_CODEC_ID    _IOW(SPRD_VPU_IOCTL_MAGIC, 14, unsigned int)
#define VPU_GET_CODEC_COUNTER    _IOWR(SPRD_VPU_IOCTL_MAGIC, 15, unsigned int)
#define VPU_SET_SCENE                _IO(SPRD_VPU_IOCTL_MAGIC, 16)
#define VPU_GET_SCENE                _IO(SPRD_VPU_IOCTL_MAGIC, 17)
#define VPU_SYNC_GSP                _IO(SPRD_VPU_IOCTL_MAGIC, 18)

#ifdef CONFIG_COMPAT
#define COMPAT_VPU_GET_IOVA    _IOWR(SPRD_VPU_IOCTL_MAGIC, 11, struct compat_iommu_map_data)
#define COMPAT_VPU_FREE_IOVA   _IOW(SPRD_VPU_IOCTL_MAGIC, 12, struct compat_iommu_map_data)

struct compat_iommu_map_data {
	compat_int_t fd;
	compat_size_t size;
	compat_ulong_t iova_addr;
};
#endif


enum vsp_version_e {
	SHARK = 0,
	DOLPHIN = 1,
	TSHARK = 2,
	SHARKL = 3,
	PIKE = 4,
	PIKEL = 5,
	SHARKL64 = 6,
	SHARKLT8 = 7,
	WHALE = 8,
	WHALE2 = 9,
	IWHALE2 = 10,
	ISHARKL2 = 11,
	SHARKL2 = 12,
	SHARKLJ1 = 13,
	SHARKLE = 14,
	PIKE2 = 15,
	SHARKL3 = 16,
	SHARKL5 = 17,
	ROC1 = 18,
	SHARKL5Pro  = 19,
	SHARKL6 = 20,
	QOGIRN6PRO = 21,
	MAX_VERSIONS,
};

struct iommu_map_data {
	int fd;
	size_t size;
	unsigned long iova_addr;
};


#endif
