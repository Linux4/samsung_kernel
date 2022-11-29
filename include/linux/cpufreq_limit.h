/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *	Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_CPUFREQ_LIMIT_H__
#define __LINUX_CPUFREQ_LIMIT_H__

enum {
	DVFS_USER_ID = 0,
	DVFS_TOUCH_ID,
	DVFS_FINGER_ID,
	DVFS_MAX_ID
};

extern void cpufreq_limit_set_table(int cpu, struct cpufreq_frequency_table *ftbl);
extern int set_freq_limit(unsigned int id, int freq);

extern unsigned int cpufreq_limit_get_over_limit(void);
extern void cpufreq_limit_set_over_limit(unsigned int val);

#endif /* __LINUX_CPUFREQ_LIMIT_H__ */
