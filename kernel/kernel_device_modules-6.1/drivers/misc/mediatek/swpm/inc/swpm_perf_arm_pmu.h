/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_PERF_ARM_PMU_H__
#define __SWPM_PERF_ARM_PMU_H__

unsigned int legacy_core_pmu_num = 3;
unsigned int legacy_dsu_pmu_num = 1;

enum swpm_perf_evt_id {
	L3DC_EVT,
	INST_SPEC_EVT,
	CYCLES_EVT,
	DSU_CYCLES_EVT,
	PMU_3_EVT,
	PMU_4_EVT,
	PMU_5_EVT,
	PMU_6_EVT,
	PMU_7_EVT,
	PMU_8_EVT,
	PMU_9_EVT,
	PMU_10_EVT,
	PMU_11_EVT,
	PMU_12_EVT,
	PMU_13_EVT,
	PMU_NUM,
};

extern unsigned int swpm_arm_pmu_get_status(void);
extern unsigned int swpm_arm_dsu_pmu_get_status(void);
extern int swpm_arm_pmu_get_idx(unsigned int evt_id,
				unsigned int cpu);
extern int swpm_arm_pmu_enable_all(unsigned int enable);
extern int swpm_arm_dsu_pmu_enable(unsigned int enable);
extern unsigned int swpm_arm_dsu_pmu_get_type(void);
extern int swpm_arm_dsu_pmu_set_type(unsigned int type);
extern void swpm_arm_ai_pmu_set(unsigned int type);
extern unsigned int swpm_arm_ai_pmu_get(void);

#endif
