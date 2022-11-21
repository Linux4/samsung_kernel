/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SYSFS_INIT_H_
#define _CAM_SYSFS_INIT_H_

#include <linux/sysfs.h>

/**
 * @brief : API to register FLASH hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int32_t cam_sysfs_init_module(void);

/**
 * @brief : API to remove FLASH Hw from platform framework.
 */
void cam_sysfs_exit_module(void);
#endif /* _CAM_SYSFS_INIT_H_ */
