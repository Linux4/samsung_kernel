/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#ifndef __GED_ASYNC_H__
#define __GED_ASYNC_H__

#define ASYNC_OK     0
#define ASYNC_ERROR  1
#define COEFF_SCAL  100000
#define RATIO_SCAL  100
#define PERF_SCAL   100
static const long async_coeff[11] = {
	-2714,
	32497,
	-42939,
	10647,
	52700,
	-61120,
	8638,
	56764,
	-64087,
	4967,
	102668
};

struct async_counter {
	long gpuactive;
	long iter;
	long compute;
	long l2ext;
	long irq;
};

struct GpuRawCounter {
	unsigned int util_active_raw;
	unsigned int util_iter_raw;
	unsigned int util_mcu_raw;
	unsigned int util_irq_raw;
	unsigned int util_sc_comp_raw;
	unsigned int util_l2ext_raw;
};

#endif /* __GED_ASYNC_H__ */
