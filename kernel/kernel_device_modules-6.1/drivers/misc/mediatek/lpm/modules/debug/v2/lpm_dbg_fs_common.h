/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_DBG_FS_COMMON_H__
#define __LPM_DBG_FS_COMMON_H__

int lpm_trace_fs_init(void);
int lpm_trace_fs_deinit(void);

int lpm_rc_fs_init(void);
int lpm_rc_fs_deinit(void);

int lpm_cpuidle_fs_init(void);
int lpm_cpuidle_fs_deinit(void);

void lpm_cpuidle_state_init(void);
void lpm_cpuidle_profile_init(void);
void lpm_cpuidle_control_init(void);

int lpm_dbg_fs_init(void);
void lpm_dbg_fs_exit(void);

unsigned int lpm_get_lp_blocked_threshold(void);

int lpm_spm_fs_init(char **str, unsigned int cnt);
int lpm_spm_fs_deinit(void);

int lpm_hwreq_fs_init(void);
int lpm_hwreq_fs_deinit(void);

#endif /* __LPM_DBG_FS_COMMON_H__ */
