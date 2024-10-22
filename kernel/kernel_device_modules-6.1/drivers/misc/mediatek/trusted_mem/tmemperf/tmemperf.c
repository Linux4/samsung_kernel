// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/cpufreq.h>
#include "tmemperf.h"

static void tmemperf_set_cpu_to_hfreq(int target_cpu, u32 high_freq,
	unsigned int freq_level_index)
{
	struct cpufreq_policy *policy;
	unsigned int index, max_index, min_index;

	if (target_cpu >= 8) {
		pr_info(PFX "invalid target cpu (%d)\n", target_cpu);
		return;
	}

	policy = cpufreq_cpu_get(target_cpu);
	if (policy == NULL) {
		pr_info(PFX "invalid policy, target cpu (%d)\n", target_cpu);
		return;
	}

	down_write(&policy->rwsem);
	max_index = 0;
	min_index = cpufreq_table_find_index_dl(policy, 0, false);
	if (high_freq) {
		/* set min_freq to selected freq */
		index = max_index + freq_level_index;
		if (index > min_index)
			index = min_index;
	} else {
		/* set min_freq to min freq */
		index = min_index;
	}
	policy->cpuinfo.min_freq = policy->freq_table[index].frequency;
	up_write(&policy->rwsem);
	cpufreq_cpu_put(policy);
	cpufreq_update_limits(target_cpu);
}

void tmemperf_set_cpu_group_to_hfreq(enum tmemperf_cpu_group group,
	u32 high_freq)
{
	unsigned int freq_level_index = 0;
	int cpu;

	if (group == CPU_SUPER_GROUP) {
		freq_level_index = SUPER_CPU_FREQ_LEVEL_INDEX;
		if (cpu_map == CPU_4_3_1_MAP) {
			tmemperf_set_cpu_to_hfreq(7, high_freq, freq_level_index);
		} else {
			for (cpu = 0; cpu < 8; cpu++)
				tmemperf_set_cpu_to_hfreq(cpu, high_freq, 0);
		}
	} else if (group == CPU_BIG_GROUP) {
		freq_level_index = BIG_CPU_FREQ_LEVEL_INDEX;
		if (cpu_map == CPU_4_3_1_MAP) {
			tmemperf_set_cpu_to_hfreq(4, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(5, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(6, high_freq, freq_level_index);
		} else if (cpu_map == CPU_6_2_MAP) {
			tmemperf_set_cpu_to_hfreq(6, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(7, high_freq, freq_level_index);
		} else {
			for (cpu = 0; cpu < 8; cpu++)
				tmemperf_set_cpu_to_hfreq(cpu, high_freq, 0);
		}
	} else if (group == CPU_LITTLE_GROUP) {
		freq_level_index = LITTLE_CPU_FREQ_LEVEL_INDEX;
		if (cpu_map == CPU_4_3_1_MAP) {
			tmemperf_set_cpu_to_hfreq(0, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(1, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(2, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(3, high_freq, freq_level_index);
		} else if (cpu_map == CPU_6_2_MAP) {
			tmemperf_set_cpu_to_hfreq(0, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(1, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(2, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(3, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(4, high_freq, freq_level_index);
			tmemperf_set_cpu_to_hfreq(5, high_freq, freq_level_index);
		} else {
			for (cpu = 0; cpu < 8; cpu++)
				tmemperf_set_cpu_to_hfreq(cpu, high_freq, 0);
		}
	} else {
		for (cpu = 0; cpu < 8; cpu++)
			tmemperf_set_cpu_to_hfreq(cpu, high_freq, 0);
	}
}
