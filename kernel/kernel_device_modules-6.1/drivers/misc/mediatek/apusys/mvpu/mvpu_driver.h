/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <apusys_core.h>

static struct apusys_device mvpu_adev;
static int mvpu_loglvl_drv;

int mvpu_init(struct apusys_core_info *info);
void mvpu_exit(void);
