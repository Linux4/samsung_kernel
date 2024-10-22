// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GPU_BM_H__
#define __GPU_BM_H__

#define GPU_BW_DEFAULT_MODE             (0)
#define GPU_BW_SPORT_MODE               (1)
#define GPU_BW_NO_PRED_MODE             (2)
#define GPU_BW_LP_MODE                  (3)
#define GPU_BM_PEAK_PERF_MODE           (4)
#define GPU_BM_PEAK_PERF_MODE_LIMIT     (5)

#define GPU_BW_RATIO_CEIL               (300)
#define GPU_BW_RATIO_FLOOR              (10)

#define GPU_BW_NO_PRED_RATIO_CEIL       (2300)
#define GPU_BW_NO_PRED_RATIO_FLOOR      (2010)

#define GPU_BM_PEAK_INDEX_TOP_LIMIT      (1)

struct v1_data {
	unsigned int version;
	unsigned int ctx;
	unsigned int frame;
	unsigned int job;
	unsigned int freq;
};

struct setupfw_t {
	phys_addr_t phyaddr;
	size_t size;
};

void MTKGPUQoS_mode(int seg_flag);
void MTKGPUQoS_setup(struct v1_data *v1, phys_addr_t phyaddr, size_t size);
int MTKGPUQoS_is_inited(void);

#endif

