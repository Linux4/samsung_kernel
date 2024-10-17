// SPDX-License-Identifier: GPL-2.0
/*
 * Exynos Mobile CPUFreq
 *
 * Copyright (C) 2021, Intel Corporation
 * Author: Samsung Electronic. S.LSI CPU Part.
 */

#include <trace/events/ems_debug.h>

#include "ems.h"

static struct cpufreq_hook_data {
	int (*func_get_next_cap)(struct tp_env *env, struct cpumask *cpus, int dst_cpu);
	int (*func_need_slack_timer)(void);
} __percpu *cpufreq_hook_data;

/* register call back when cpufreq governor initialized */
void cpufreq_register_hook(int cpu,
		int (*func_get_next_cap)(struct tp_env *env, struct cpumask *cpus, int dst_cpu),
		int (*func_need_slack_timer)(void))
{
	struct cpufreq_hook_data *data = per_cpu_ptr(cpufreq_hook_data, cpu);

	if (!data)
		return;

	data->func_get_next_cap = func_get_next_cap;
	data->func_need_slack_timer = func_need_slack_timer;
}

/* unregister call back when cpufreq governor initialized */
void cpufreq_unregister_hook(int cpu)
{
	struct cpufreq_hook_data *data = per_cpu_ptr(cpufreq_hook_data, cpu);

	if (!data)
		return;

	data->func_get_next_cap = NULL;
	data->func_need_slack_timer = NULL;
}

/******************************************************************************
 *                           Next capacity callback                           *
 ******************************************************************************/
int cpufreq_get_next_cap(struct tp_env *env, struct cpumask *cpus, int dst_cpu)
{
	struct cpufreq_hook_data *data = per_cpu_ptr(cpufreq_hook_data, cpumask_any(cpus));

	if (data && data->func_get_next_cap) {
		int cap = data->func_get_next_cap(env, cpus, dst_cpu);

		if (cap > 0)
			return cap;
	}

	return -1;
}

/******************************************************************************
 *                                Slack timer                                 *
 ******************************************************************************/
static struct timer_list __percpu *slack_timer;

static void slack_timer_add(struct timer_list *timer, int cpu)
{
	struct cpufreq_hook_data *data = per_cpu_ptr(cpufreq_hook_data, cpu);

	if (!data || !data->func_need_slack_timer)
		return;

	if (data->func_need_slack_timer()) {
		/* slack expired time = 20ms */
		timer->expires = jiffies + msecs_to_jiffies(20);
		add_timer_on(timer, cpu);
	}
}

void slack_timer_cpufreq(int cpu, bool idle_start, bool idle_end)
{
	struct timer_list *timer = per_cpu_ptr(slack_timer, cpu);

	if (idle_start == idle_end)
		return;

	if (timer_pending(timer))
		del_timer_sync(timer);

	if (idle_start)
		slack_timer_add(timer, cpu);
}

static void slack_timer_func(struct timer_list *timer)
{
	int cpu = smp_processor_id();

	/*
	 * The purpose of slack-timer is to wake up the CPU from IDLE, in order
	 * to decrease its frequency if it is not set to minimum already.
	 *
	 * This is important for platforms where CPU with higher frequencies
	 * consume higher power even at IDLE.
	 */
	trace_slack_timer(cpu);

	/* If slack timer is still needed, add slack timer again */
	slack_timer_add(timer, cpu);
}

static void slack_timer_init(void)
{
	int cpu;

	slack_timer = alloc_percpu(struct timer_list);

	for_each_possible_cpu(cpu)
		timer_setup(per_cpu_ptr(slack_timer, cpu), slack_timer_func, TIMER_PINNED);
}

/******************************************************************************
 *                                 Freq clamp                                 *
 ******************************************************************************/
#define FCLAMP_MIN	0
#define FCLAMP_MAX	1

static struct fclamp {
	struct fclamp_data fclamp_min;
	struct fclamp_data fclamp_max;
} __percpu **fclamp;

#define per_cpu_fc(cpu)		(*per_cpu_ptr(fclamp, cpu))

static struct fclamp_data boost_fcd;
static int fclamp_monitor_group[CGROUP_COUNT];
static int fclamp_boost;

static int fclamp_can_release(int cpu, struct fclamp_data *fcd, unsigned int boost)
{
	int length = fcd->target_period;
	int target_ratio = fcd->target_ratio;
	int type = fcd->type;
	int active_ratio = mlt_cpu_value(cpu, length, AVERAGE, ACTIVE);

	/* convert ratio normalized to 1024 to percentage and get average */
	active_ratio = active_ratio * 100 / SCHED_CAPACITY_SCALE;

	/*
	 * Release fclamp max if cpu is busier than target and
	 * release fclamp min if cpu is more idle than target.
	 */

	if (type == FCLAMP_MAX && active_ratio > target_ratio)
		return 1;
	else if (type == FCLAMP_MIN && active_ratio < target_ratio)
		return 1;

	return 0;
}

unsigned int fclamp_apply(struct cpufreq_policy *policy, unsigned int orig_freq)
{
	struct fclamp *fc = per_cpu_fc(policy->cpu);
	struct fclamp_data *fcd;
	int cpu, type;
	unsigned int new_freq, count = 0, boost = fclamp_boost;

	/* Select fclamp data according to boost or origin util */
	if (boost) {
		/*
		 * If orig freq is same as max freq, it does not need to
		 * handle fclamp boost
		 */
		if (orig_freq == policy->cpuinfo.max_freq)
			return orig_freq;
		fcd = &boost_fcd;
	} else if (fc->fclamp_max.freq && (orig_freq > fc->fclamp_max.freq)) {
		fcd = &fc->fclamp_max;
	} else if (fc->fclamp_min.freq && (orig_freq < fc->fclamp_min.freq)) {
		fcd = &fc->fclamp_min;
	} else {
		return orig_freq;
	}

	if (!fcd->target_period)
		return orig_freq;

	type = fcd->type;
	new_freq = fcd->freq;

	for_each_cpu(cpu, policy->cpus) {
		/* check whether release clamping or not */
		if (fclamp_can_release(cpu, fcd, boost)) {
			count++;

			if (type == FCLAMP_MAX)
				break;
		}
	}

	/* Release fclamp min when all cpus in policy are idler than target */
	if (type == FCLAMP_MIN && count == cpumask_weight(policy->cpus))
		new_freq = orig_freq;
	/* Release fclamp max when any cpu in policy is busier than target */
	else if (type == FCLAMP_MAX && count > 0)
		new_freq = orig_freq;

	new_freq = clamp_val(new_freq, policy->cpuinfo.min_freq, policy->cpuinfo.max_freq);
	trace_fclamp_apply(policy->cpu, orig_freq, new_freq, boost, fcd);

	return new_freq;
}

static int fclamp_emstune_notifier_call(struct notifier_block *nb, unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int i, cpu;

	for_each_possible_cpu(cpu) {
		struct fclamp *fc = per_cpu_fc(cpu);

		fc->fclamp_min.freq = cur_set->fclamp.min_freq[cpu];
		fc->fclamp_min.target_period = cur_set->fclamp.min_target_period[cpu];
		fc->fclamp_min.target_ratio = cur_set->fclamp.min_target_ratio[cpu];

		fc->fclamp_max.freq = cur_set->fclamp.max_freq[cpu];
		fc->fclamp_max.target_period = cur_set->fclamp.max_target_period[cpu];
		fc->fclamp_max.target_ratio = cur_set->fclamp.max_target_ratio[cpu];
	}

	for (i = 0; i < CGROUP_COUNT; i++)
		fclamp_monitor_group[i] = cur_set->fclamp.monitor_group[i];

	return NOTIFY_OK;
}

static struct notifier_block fclamp_emstune_notifier = {
	.notifier_call = fclamp_emstune_notifier_call,
};

static int fclamp_sysbusy_notifier_call(struct notifier_block *nb, unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	fclamp_boost = state;

	return NOTIFY_OK;
}

static struct notifier_block fclamp_sysbusy_notifier = {
	.notifier_call = fclamp_sysbusy_notifier_call,
};

static int fclamp_init(void)
{
	int cpu;

	fclamp = alloc_percpu(struct fclamp *);
	if (!fclamp) {
		pr_err("failed to allocate fclamp\n");
		return -ENOMEM;
	}

	for_each_possible_cpu(cpu) {
		struct fclamp *fc;
		int i;

		if (cpu != cpumask_first(cpu_clustergroup_mask(cpu)))
			continue;

		fc = kzalloc(sizeof(struct fclamp), GFP_KERNEL);
		if (!fc)
			return -ENOMEM;

		fc->fclamp_min.type = FCLAMP_MIN;
		fc->fclamp_max.type = FCLAMP_MAX;

		for_each_cpu(i, cpu_clustergroup_mask(cpu))
			per_cpu_fc(i) = fc;
	}

	/* monitor topapp as default */
	fclamp_monitor_group[CGROUP_TOPAPP] = 1;

	boost_fcd.freq = INT_MAX;
	boost_fcd.target_period = 1;
	boost_fcd.target_ratio = 80;
	boost_fcd.type = FCLAMP_MIN;

	emstune_register_notifier(&fclamp_emstune_notifier);
	sysbusy_register_notifier(&fclamp_sysbusy_notifier);

	return 0;
}

int cpufreq_init(void)
{
	cpufreq_hook_data = alloc_percpu(struct cpufreq_hook_data);
	if (!cpufreq_hook_data) {
		pr_err("Failed to allocate cpufreq_hook_data\n");
		return -ENOMEM;
	}

	slack_timer_init();
	fclamp_init();

	return 0;
}

