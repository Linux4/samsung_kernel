/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __AOV_RECOVERY_V1_H__
#define __AOV_RECOVERY_V1_H__

#include <linux/types.h>
#include <linux/platform_device.h>

int aov_recovery_v1_init(void);

unsigned int get_aov_recovery_state(void);

void aov_recovery_v1_exit(void);

#endif // __AOV_RECOVERY_V1_H__
