/*
 * Copyright (c) 2016 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos ACME(A Cpufreq that Meets Every chipset) driver implementation
 */

#include <linux/pm_qos.h>
#include <soc/samsung/exynos-dm.h>
#include <linux/timer.h>

#define CPUFREQ_RESTART 0x1 << 0
#define CPUFREQ_SUSPEND 0x1 << 1

struct exynos_slack_timer {
	/* for slack timer */
	unsigned long min;
	int enabled;
	int expired_time;
	struct timer_list timer;
};

struct exynos_cpufreq_dm {
	struct list_head		list;
	struct exynos_dm_constraint	c;
};

typedef int (*target_fn)(struct cpufreq_policy *policy,
			        unsigned int target_freq,
			        unsigned int relation);

struct exynos_cpufreq_ready_block {
	struct list_head		list;

	/* callback function to update policy-dependant data */
	int (*update)(struct cpufreq_policy *policy);
	int (*get_target)(struct cpufreq_policy *policy, target_fn target);
};

struct exynos_cpufreq_domain {
	/* list of domain */
	struct list_head		list;

	/* lock */
	struct mutex			lock;

	/* dt node */
	struct device_node		*dn;

	/* domain identity */
	unsigned int			id;
	struct cpumask			cpus;
	unsigned int			cal_id;
	int				dm_type;

	/* frequency scaling */
	bool				enabled;

	unsigned int			table_size;
	struct cpufreq_frequency_table	*freq_table;

	unsigned int			max_freq;
	unsigned int			min_freq;
	unsigned int			boot_freq;
	unsigned int			resume_freq;
	unsigned int			old;

	/* PM QoS class */
	unsigned int			pm_qos_min_class;
	unsigned int			pm_qos_max_class;
	struct pm_qos_request		min_qos_req;
	struct pm_qos_request		max_qos_req;
	struct notifier_block		pm_qos_min_notifier;
	struct notifier_block		pm_qos_max_notifier;

	struct pm_qos_request		user_qos_min_req;
	struct pm_qos_request		user_qos_max_req;

	/* for sysfs */
	unsigned int			user_boost;

	/* freq boost */
	bool				boost_supported;
	unsigned int			*boost_max_freqs;
	struct cpumask			online_cpus;

	/* list head of DVFS Manager constraints */
	struct list_head		dm_list;

	bool				need_awake;

	struct thermal_cooling_device *cdev;
};

/*
 * list head of cpufreq domain
 */

extern struct exynos_cpufreq_domain
		*find_domain_cpumask(const struct cpumask *mask);
extern struct list_head *get_domain_list(void);

/*
 * the time it takes on this CPU to switch between
 * two frequencies in nanoseconds
 */
#define TRANSITION_LATENCY	5000000

/*
 * Exynos CPUFreq API
 */
extern void exynos_cpufreq_ready_list_add(struct exynos_cpufreq_ready_block *rb);
extern unsigned int exynos_pstate_get_boost_freq(int cpu);
