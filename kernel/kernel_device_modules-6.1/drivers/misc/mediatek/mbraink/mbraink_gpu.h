/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef MBRAINK_GPU_H
#define MBRAINK_GPU_H
#include "mbraink_ioctl_struct_def.h"

extern int mbraink_netlink_send_msg(const char *msg);

int mbraink_gpu_init(void);
int mbraink_gpu_deinit(void);
void mbraink_gpu_setQ2QTimeoutInNS(unsigned long long q2qTimeoutInNS);
unsigned long long mbraink_gpu_getQ2QTimeoutInNS(void);
int mbraink_gpu_getOppInfo(struct mbraink_gpu_opp_info *gOppInfo);
int mbraink_gpu_getStateInfo(struct mbraink_gpu_state_info *gStateInfo);
int mbraink_gpu_getLoadingInfo(struct mbraink_gpu_loading_info *gLoadingInfo);


void mbraink_gpu_setPerfIdxTimeoutInNS(unsigned long long perfIdxTimeoutInNS);
void mbraink_gpu_setPerfIdxLimit(int perfIdxLimit);
void mbraink_gpu_dumpPerfIdxList(void);

#if IS_ENABLED(CONFIG_MTK_FPSGO_V3) || IS_ENABLED(CONFIG_MTK_FPSGO)

void fpsgo2mbrain_hint_frameinfo(int pid, unsigned long long bufID,
	int fps, unsigned long long time);

void fpsgo2mbrain_hint_perfinfo(int pid, unsigned long long bufID,
	int perf_idx, int sbe_ctrl, unsigned long long ts);

void fpsgo2mbrain_hint_deleteperfinfo(int pid, unsigned long long bufID,
	int perf_idx, int sbe_ctrl, unsigned long long ts);

#endif

#endif /*end of MBRAINK_GPU_H*/
