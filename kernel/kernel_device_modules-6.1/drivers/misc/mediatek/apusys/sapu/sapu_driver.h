/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#ifndef __SAPU_DRIVER_H_
#define __SAPU_DRIVER_H_

#include "sapu_plat.h"

#define APUSYS_SAPU_IOC_MAGIC	'S'
#define APUSYS_SAPU_DATAMEM	_IOWR(APUSYS_SAPU_IOC_MAGIC, 1, \
				      struct sapu_datamem_info)
#define APUSYS_POWER_CONTROL	_IOWR(APUSYS_SAPU_IOC_MAGIC, 2, u32)

struct sapu_ha_tranfer {
	uint64_t handle;
	dma_addr_t dma_addr;
	uint64_t model_hd_ha;
};

long apusys_sapu_internal_ioctl(struct file *filep, unsigned int cmd,
				void __user *arg, unsigned int compat);
struct sapu_private *get_sapu_private(void);
struct sapu_lock_rpmsg_device *get_rpm_dev(void);
struct mutex *get_rpm_mtx(void);
int *get_lock_ref_cnt(void);

#endif // __SAPU_DRIVER_H_
