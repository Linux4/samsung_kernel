/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#define SUPER_CPU_FREQ_LEVEL_INDEX	0
#define BIG_CPU_FREQ_LEVEL_INDEX	0
#define LITTLE_CPU_FREQ_LEVEL_INDEX	0

#define PFX	"[TMEMPERF]: "

extern u32 cpu_map;

enum tmemperf_cpu_map {
	CPU_4_3_1_MAP = 1,
	CPU_6_2_MAP = 2
};

enum tmemperf_cpu_group {
	CPU_SUPER_GROUP = 1,
	CPU_BIG_GROUP = 2,
	CPU_LITTLE_GROUP = 3
};

void tmemperf_set_cpu_group_to_hfreq(enum tmemperf_cpu_group group, u32 high_freq);
