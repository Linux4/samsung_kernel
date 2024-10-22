/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#ifndef MBRAINK_CPUFREQ_H
#define MBRAINK_CPUFREQ_H

#include "mbraink_ioctl_struct_def.h"

#define CPUFREQ_NOTIFY_SZ   32
/*The cluster size will be dependent to platform, DUJAC is 3*/
#define CPU_CLUSTER_SZ      3

struct mbraink_cpufreq_notify {
	long long timestamp;
	int cid;
	unsigned short qos_type;
	unsigned int freq_limit;
	char caller[MAX_FREQ_SZ];
	bool dirty;
};

struct mbraink_cpufreq_notify_data {
	unsigned long notify_count;
	struct mbraink_cpufreq_notify drv_data[CPUFREQ_NOTIFY_SZ];
};

int mbraink_cpufreq_notify_init(void);
void mbraink_cpufreq_notify_exit(void);
void mbraink_get_cpufreq_notifier_info(
		unsigned short current_cluster_idx,
		unsigned short current_idx,
		struct mbraink_cpufreq_notify_struct_data *cpufreq_notify_buffer);
void remove_freq_qos_hook(void);
#endif
