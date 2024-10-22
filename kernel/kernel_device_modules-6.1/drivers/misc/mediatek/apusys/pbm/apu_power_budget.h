/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_APU_POWER_BUDGET_H__
#define __MTK_APU_POWER_BUDGET_H__

enum pbm_mode {
	PBM_MODE_NORMAL,
	PBM_MODE_PERFORMANCE,
	PBM_MODE_MAX,
};

/*
 * mode : which mode you want to config
 * counter : 1 means mode begin, 0 means mode end
 * return int : 0 means request is succeeded, other values are failed
 */
int apu_power_budget(enum pbm_mode mode, int counter);

/*
 * mode : which mode you want to check
 * return int : current counter in this mode
 */
int apu_power_budget_checker(enum pbm_mode mode);

#define PBM_NORMAL_PWR			(9000) // 9w
#define PBM_PERFORMANCE_PWR		(9000) // 9w

#define NORM_OFF_DELAY	(20) // ms
#define PERF_OFF_DELAY	(20) // ms

#endif // __MTK_APU_POWER_BUDGET_H__
