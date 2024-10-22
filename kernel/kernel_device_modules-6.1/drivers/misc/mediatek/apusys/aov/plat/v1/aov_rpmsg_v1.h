/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __AOV_RPMSG_V1_H__
#define __AOV_RPMSG_V1_H__

#include <linux/types.h>
#include <linux/platform_device.h>

int aov_rpmsg_v1_init(void);

void aov_rpmsg_v1_exit(void);

#endif // __AOV_RPMSG_V1_H__
