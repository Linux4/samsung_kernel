/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *	Minsung Kim <ms925.kim@samsung.com>
 *
 * Modified by: Praveen BP <bp.praveen@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_CPUFREQ_LIMIT_H__
#define __LINUX_CPUFREQ_LIMIT_H__

struct cpufreq_limit {
	struct list_head node;
	char name[20];
	unsigned long min;
	unsigned long max;
};

#ifdef CONFIG_CPU_FREQ_LIMIT
struct cpufreq_limit *cpufreq_limit_get(const char *name,
				unsigned long min, unsigned long max);
int cpufreq_limit_put(struct cpufreq_limit *handle);

static inline
struct cpufreq_limit *limit_min_cpufreq(const char *name, unsigned long min)
{
	return cpufreq_limit_get(name, min, 0);
}

static inline
struct cpufreq_limit *limit_max_cpufreq(const char *name, unsigned long max)
{
	return cpufreq_limit_get(name, 0, max);
}
#ifdef CONFIG_SCHED_HMP
ssize_t cpufreq_limit_get_table(char *buf);
#endif
#else

static inline
struct cpufreq_limit *cpufreq_limit_get(const char *label,
				unsigned long min, unsigned long max);
{
	return NULL;
}

int cpufreq_limit_put(struct cpufreq_limit_handle *handle)
{
	return 0;
}

static inline
struct cpufreq_limit *limit_min_cpufreq(const char *name, unsigned long min)
{
	return NULL;
}

static inline
struct cpufreq_limit *limit_max_cpufreq(const char *name, unsigned long max)
{
	return NULL;
}
#endif
#endif /* __LINUX_CPUFREQ_LIMIT_H__ */
