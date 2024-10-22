/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __AOV_RECOVERY_V2_H__
#define __AOV_RECOVERY_V2_H__

#include <linux/types.h>
#include <linux/platform_device.h>

int aov_recovery_v2_init(struct platform_device *pdev);

void aov_recovery_v2_exit(struct platform_device *pdev);

#endif // __AOV_RECOVERY_V2_H__
