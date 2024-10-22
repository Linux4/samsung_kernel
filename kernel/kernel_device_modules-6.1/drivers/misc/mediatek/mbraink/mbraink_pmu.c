// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include "mbraink_pmu.h"

extern DEFINE_PER_CPU(struct perf_event *, inst_spec_events);
static DEFINE_PER_CPU(unsigned long long,  inst_spec_count);
static u64 spec_instructions[8];

static int mbraink_pmu_get_inst_count(int cpu)
{
	struct perf_event *event = per_cpu(inst_spec_events, cpu);
	unsigned long long new = 0;
	unsigned long long old = per_cpu(inst_spec_count, cpu);
	unsigned long long diff = 0;

	if (event && event->state == PERF_EVENT_STATE_ACTIVE && cpu_online(cpu)) {
		perf_event_read_local(event, &new, NULL, NULL);
		if (new > old && old > 0)
			diff = (unsigned long long)(new - old);

		per_cpu(inst_spec_count, cpu) = new;
		spec_instructions[cpu] += diff;

		pr_info("perf event read on CPU %d, new: %llu, old: %llu, diff: %llu\n", cpu, new, old, diff);
	}

	return diff;
}

static void _mbraink_gen_pmu_count(void *val)
{
	int cpu;

	cpu = smp_processor_id();
	mbraink_pmu_get_inst_count(cpu);
}

#if IS_ENABLED(CONFIG_MTK_LPM_MT6989)
int mbraink_enable_pmu_inst_spec(bool enable)
{
	pr_info("mbrain enable pmu inst spec, enable: %d", enable);
	if (enable) {
		/* read perf event from swpm, func enable reset value only */
		spec_instructions[0] = 0;
		spec_instructions[1] = 0;
		spec_instructions[2] = 0;
		spec_instructions[3] = 0;
		spec_instructions[4] = 0;
		spec_instructions[5] = 0;
		spec_instructions[6] = 0;
		spec_instructions[7] = 0;
	}
	return 0;
}

int mbraink_get_pmu_inst_spec(struct mbraink_pmu_info *pmuInfo)
{
	int cpu;
	unsigned int val[2];

	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu, _mbraink_gen_pmu_count, val, 1);
	}

	pmuInfo->cpu0_pmu_data_inst_spec = spec_instructions[0];
	pmuInfo->cpu1_pmu_data_inst_spec = spec_instructions[1];
	pmuInfo->cpu2_pmu_data_inst_spec = spec_instructions[2];
	pmuInfo->cpu3_pmu_data_inst_spec = spec_instructions[3];
	pmuInfo->cpu4_pmu_data_inst_spec = spec_instructions[4];
	pmuInfo->cpu5_pmu_data_inst_spec = spec_instructions[5];
	pmuInfo->cpu6_pmu_data_inst_spec = spec_instructions[6];
	pmuInfo->cpu7_pmu_data_inst_spec = spec_instructions[7];

	pr_notice("%s: get inst spec: %llu %llu %llu %llu %llu %llu %llu %llu", __func__,
	spec_instructions[0], spec_instructions[1],
	spec_instructions[2], spec_instructions[3],
	spec_instructions[4], spec_instructions[5],
	spec_instructions[6], spec_instructions[7]);

	return 0;
}

#else
int mbraink_enable_pmu_inst_spec(bool enable)
{
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}

int mbraink_get_pmu_inst_spec(struct mbraink_pmu_info *pmuInfo)
{
	pmuInfo->cpu0_pmu_data_inst_spec = spec_instructions[0];
	pmuInfo->cpu1_pmu_data_inst_spec = spec_instructions[1];
	pmuInfo->cpu2_pmu_data_inst_spec = spec_instructions[2];
	pmuInfo->cpu3_pmu_data_inst_spec = spec_instructions[3];
	pmuInfo->cpu4_pmu_data_inst_spec = spec_instructions[4];
	pmuInfo->cpu5_pmu_data_inst_spec = spec_instructions[5];
	pmuInfo->cpu6_pmu_data_inst_spec = spec_instructions[6];
	pmuInfo->cpu7_pmu_data_inst_spec = spec_instructions[7];
	pr_info("%s: not enable do nothing\n", __func__);
	return 0;
}
#endif
