/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __PLAT_SYS_RES_SIGNAL_H__
#define __PLAT_SYS_RES_SIGNAL_H__

#include <swpm_v6989_ext.h>

enum lpm_sys_res_record_plat_op_id {
	SYS_RES_DURATION = 0,
	SYS_RES_SUSPEND_TIME,
	SYS_RES_SIG_TIME,
	SYS_RES_SIG_ID,
	SYS_RES_SIG_GROUP_ID,
	SYS_RES_SIG_OVERALL_RATIO,
	SYS_RES_SIG_SUSPEND_RATIO,
	SYS_RES_SIG_ADDR,
};

extern struct sys_res_group_info sys_res_group_info[NR_SPM_GRP];

int lpm_sys_res_plat_init(void);
void lpm_sys_res_plat_deinit(void);

#endif


