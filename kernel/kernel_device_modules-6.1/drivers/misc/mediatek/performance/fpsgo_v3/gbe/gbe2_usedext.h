/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef GBE_USEDEXT_H
#define GBE_USEDEXT_H
#include "fpsgo_base.h"

extern int gbe2xgf_get_dep_list_num(int pid, unsigned long long bufID);
struct gbe_runtime {
	int pid;
	unsigned long long runtime;
	unsigned long long loading;
};
extern int gbe2xgf_get_dep_list(int pid, int count,
	struct gbe_runtime *arr, unsigned long long bufID);

#endif

