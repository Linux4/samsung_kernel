/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __GED_GPU_SLC_H__
#define __GED_GPU_SLC_H__
#include <linux/types.h>
#include "ged_type.h"


struct gpu_slc_stat {
	unsigned int mode;
	unsigned int policy_0_hit_rate_r;
	unsigned int policy_1_hit_rate_r;
	unsigned int policy_2_hit_rate_r;
	unsigned int policy_3_hit_rate_r;
	unsigned int policy;
	unsigned int hit_rate_r;
	unsigned int isoverflow;
};

static GED_ERROR gpu_slc_sysram_init(void);
GED_ERROR ged_gpu_slc_init(void);
struct gpu_slc_stat *get_gpu_slc_stat(void);
void ged_gpu_slc_dynamic_mode(unsigned int idx);
GED_ERROR ged_gpu_slc_exit(void);

#endif /* __GED_GPU_SLC_H__ */

