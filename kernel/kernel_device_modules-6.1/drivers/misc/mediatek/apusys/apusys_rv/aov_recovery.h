/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __AOV_RECOVERY_H__
#define __AOV_RECOVERY_H__

#include <linux/types.h>
#include "apusys_core.h"
#include "apu.h"

int aov_recovery_ipi_init(struct platform_device *pdev, struct mtk_apu *apu);
void aov_recovery_exit(void);
int aov_recovery_cb(void);
void aov_recovery_exit(void);

#endif // __AOV_RECOVERY_H__
