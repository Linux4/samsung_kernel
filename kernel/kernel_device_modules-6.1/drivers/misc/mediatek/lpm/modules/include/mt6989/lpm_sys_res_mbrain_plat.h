/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __PLAT_SYS_RES_MBRAIN_EXT_H__
#define __PLAT_SYS_RES_MBRAIN_EXT_H__

#define SYS_RES_ALL_DATA_MODULE_ID (0)
#define SYS_RES_LVL_DATA_MODULE_ID (2)
#define SYS_RES_DATA_VERSION (0)

int lpm_sys_res_mbrain_plat_init (void);
void lpm_sys_res_mbrain_plat_deinit (void);

#endif
