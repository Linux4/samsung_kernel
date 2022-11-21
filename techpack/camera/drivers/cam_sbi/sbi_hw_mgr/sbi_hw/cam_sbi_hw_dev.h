/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SBI_HW_DEV_H_
#define _CAM_SBI_HW_DEV_H_

// #include "cam_debug_util.h"
// #include "cam_sbi_hw_mgr_intf.h"

/**
 * @brief : API to register SBI hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_sbi_hw_init_module(void);

/**
 * @brief : API to remove SBI hw interface from platform framework.
 */
void cam_sbi_hw_exit_module(void);

#endif /*_CAM_SBI_HW_DEV_H_ */