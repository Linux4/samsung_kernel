/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __AOV_PLAT_H__
#define __AOV_PLAT_H__

#include <linux/types.h>
#include <linux/platform_device.h>

int aov_plat_init(struct platform_device *pdev, unsigned int version);

void aov_plat_exit(struct platform_device *pdev, unsigned int version);

#endif // __AOV_PLAT_H__
