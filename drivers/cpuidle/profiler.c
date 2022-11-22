/*
 * Copyright (c) 2018 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * CPUIDLE profiler for Exynos
 */

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/kdev_t.h>

#include <soc/samsung/exynos-cpupm.h>
#include <soc/samsung/exynos_perf_cpuidle.h>

/* whether profiling has started */
static bool profile_started;
static bool auto_profile_started;

/*
 * Represents statistic of idle state.
 * All idle states are mapped 1:1 with cpuidle_stats.
 */
struct cpuidle_stats {
	/* time to enter idle state */
	ktime_t			idle_entry_time;

	/* number of times an idle state is entered */
	unsigned int		entry_count;

	/* number of times the entry into idle state is canceled */
	unsigned int		cancel_count;

	/* time in idle state */
	unsigned long long	time;
};

/* description length of idle state */
#define DESC_LEN	32

/*
 * Manages idle state where cpu enters individually. One cpu_idle_state
 * structure manages a idle state for each cpu to enter, and the number
 * of structure is determined by cpuidle driver.
 */
struct cpu_idle_state {
	/* description of idle state */
	char			desc[DESC_LEN];

	/* idle state statstics for each cpu */
	struct cpuidle_stats	stats[NR_CPUS];
};

/* cpu idle state list and length of cpu idle state list */
static struct cpu_idle_state *cpu_idle_state;
static int cpu_idle_state_count;

/*
 * Manages idle state in which multiple cpus unit enter. Each idle state
 * has one group_idle_state structure.
 */
struct group_idle_state {
	/* idle state id, it must be unique */
	int			id;

	/* description of idle state */
	char 			desc[DESC_LEN];

	/* idle state statstics */
	struct cpuidle_stats	stats;
};

/*
 * To easily manage group_idle_state dynamically, manage the list as an
 * list. Currently, the maximum number of group idle states supported is 5,
 * which is unlikely to exceed the number of states empirically.
 */
#define MAX_GROUP_IDLE_STATE	5

/* group idle state list and length of group idle state list */
static struct group_idle_state *group_idle_state[MAX_GROUP_IDLE_STATE];
static int group_idle_state_count;

/* delayed_work for cpuidle auto profiling */
struct delayed_work auto_profile_work;

/************************************************************************
 *                              Profiling                               *
 ************************************************************************/
static void idle_enter(struct cpuidle_stats *stats)
{
	stats->idle_entry_time = ktime_get();
	stats->entry_count++;
}

static void idle_exit(struct cpuidle_stats *stats, int cancel)
{
	s64 diff;

	/*
	 * If profiler is started with cpu already in idle state,
	 * idle_entry_time is 0 because entry event is not recorded.
	 * From the start of the profile to cpu wakeup is the idle time,
	 * but ignore this because it is complex to handle it and the
	 * time is not large.
	 */
	if (!stats->idle_entry_time)
		return;

	if (cancel) {
		stats->cancel_count++;
		return;
	}

	diff = ktime_to_us(ktime_sub(ktime_get(), stats->idle_entry_time));
	stats->time += diff;

	stats->idle_entry_time = 0;
}

/*
 * cpuidle_profile_cpu_idle_enter/cpuidle_profile_cpu_idle_exit
 * : profilie for cpu idle state
 */
void cpuidle_profile_cpu_idle_enter(int cpu, int index)
{
	if (!profile_started)
		return;

	idle_enter(&cpu_idle_state[index].stats[cpu]);
	exynos_perf_cpu_idle_enter(cpu, index);
}

void cpuidle_profile_cpu_idle_exit(int cpu, int index, int cancel)
{
	if (!profile_started)
		return;

	exynos_perf_cpu_idle_exit(cpu, index, cancel);
	idle_exit(&cpu_idle_state[index].stats[cpu], cancel);
}

/*
 * cpuidle_profile_group_idle_enter/cpuidle_profile_group_idle_exit
 * : profilie for group idle state
 */
void cpuidle_profile_group_idle_enter(int id)
{
	int i;

	if (!profile_started)
		return;

	for (i = 0; i < group_idle_state_count; i++)
		if (group_idle_state[i]->id == id)
			break;

	idle_enter(&group_idle_state[i]->stats);
}

void cpuidle_profile_group_idle_exit(int id, int cancel)
{
	int i;

	if (!profile_started)
		return;

	for (i = 0; i < group_idle_state_count; i++)
		if (group_idle_state[i]->id == id)
			break;

	idle_exit(&group_idle_state[i]->stats, cancel);
}

/************************************************************************
 *                          Profile start/stop                          *
 ************************************************************************/
/* totoal profiling time */
static s64 profile_time;

/* start time of profile */
static ktime_t profile_start_time;

/* idle-ip */
#define FIX_IDLE_IP_MAX			32
extern struct list_head idle_ip_list;
extern struct fix_idle_ip fix_idle_ip_arr[];

static void clear_stats(struct cpuidle_stats *stats)
{
	if (!stats)
		return;

	stats->idle_entry_time = 0;

	stats->entry_count = 0;
	stats->cancel_count = 0;
	stats->time = 0;
}

static void reset_profile(void)
{
	struct idle_ip *ip;
	int cpu, i;

	profile_start_time = 0;

	for (i = 0; i < cpu_idle_state_count; i++)
		for_each_possible_cpu(cpu)
			clear_stats(&cpu_idle_state[i].stats[cpu]);

	for (i = 0; i < group_idle_state_count; i++)
		clear_stats(&group_idle_state[i]->stats);

	/* Initialize idle-ip count */
	for (i = 0; i < FIX_IDLE_IP_MAX; i++)
		fix_idle_ip_arr[i].count = 0;

	list_for_each_entry(ip, &idle_ip_list, list)
		ip->count = 0;
}

static void do_nothing(void *unused)
{
}

static void cpuidle_profile_start(void)
{
	if (profile_started) {
		pr_err("CPUIDLE: Profile is ongoing\n");
		return;
	}

	exynos_perf_cpuidle_start();

	reset_profile();
	profile_start_time = ktime_get();

	profile_started = 1;

	preempt_disable();
	/* wakeup all cpus to start profile */
	smp_call_function(do_nothing, NULL, 1);
	preempt_enable();

	pr_info("CPUIDLE: Profile start!!\n");
}

static void cpuidle_profile_stop(void)
{
	if (!profile_started) {
		pr_err("CPUIDLE: Profile does not start yet\n");
		return;
	}

	if (auto_profile_started) {
		pr_err("CPUIDLE: Auto Profile is ongoing\n");
		return;
	}

	exynos_perf_cpuidle_stop();

	pr_info("CPUIDLE: Profile is done!!\n");

	preempt_disable();
	/* wakeup all cpus to stop profile */
	smp_call_function(do_nothing, NULL, 1);
	preempt_enable();

	profile_started = 0;

	profile_time = ktime_to_us(ktime_sub(ktime_get(), profile_start_time));
}

static void cpuidle_auto_profile_start(int timeout)
{
	if (profile_started) {
		pr_err("CPUIDLE: Profile is ongoing\n");
		return;
	}

	cpuidle_profile_start();
	auto_profile_started = 1;
	schedule_delayed_work(&auto_profile_work,
			msecs_to_jiffies(timeout * MSEC_PER_SEC));
}

static void cpuidle_auto_profile_stop(void)
{
	if (!profile_started) {
		pr_err("CPUIDLE: Profile does not start yet\n");
		return;
	}

	cancel_delayed_work_sync(&auto_profile_work);
	auto_profile_started = 0;
	cpuidle_profile_stop();
}

static void print_result(void);
static void cpuidle_auto_profile_work(struct work_struct *work)
{
	auto_profile_started = 0;
	cpuidle_profile_stop();
	print_result();
}

/************************************************************************
 *                               IDLE IP                                *
 ************************************************************************/
void cpuidle_profile_idle_ip(unsigned long long val)
{
	struct idle_ip *ip;

	/*
	 * Return if profile is not started
	 */
	if (!profile_started)
		return;

	list_for_each_entry(ip, &idle_ip_list, list) {
		/*
		 * Profile non-idle IP using @val
		 *
		 * A bit of val is
		 *                      == 0, it means idle.
		 *                      == 1, it means non-idle.
		 * So, profiler only count IP with a bit value of 1.
		 */
		if (val & ((unsigned long long)1 << ip->index))
			ip->count++;
	}
}

void cpuidle_profile_fix_idle_ip(unsigned int fix_idle_ip, int max_index)
{
	int i;
	unsigned int val = 1;

	/*
	 * Return if profile is not started
	 */
	if (!profile_started)
		return;

	for (i = 0; i < max_index; i++)
		if (fix_idle_ip & val << i)
			fix_idle_ip_arr[i].count++;
}

/************************************************************************
 *                              Show result                             *
 ************************************************************************/
static int calculate_percent(s64 residency)
{
	if (!residency)
		return 0;

	residency *= 100;
	do_div(residency, profile_time);

	return residency;
}

static unsigned long long cpu_idle_time(int cpu)
{
	unsigned long long idle_time = 0;
	int i;

	for (i = 0; i < cpu_idle_state_count; i++)
		idle_time += cpu_idle_state[i].stats[cpu].time;

	return idle_time;
}

static int cpu_idle_ratio(int cpu)
{
	return calculate_percent(cpu_idle_time(cpu));
}

static ssize_t show_result(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct idle_ip *ip;
	int ret = 0;
	int cpu, i;

	if (profile_started)
		return snprintf(buf, PAGE_SIZE, "CPUIDLE: Profile is ongoing\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#############################################################\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"Profiling Time : %lluus\n", profile_time);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"\n[total idle ratio]\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#cpu      #time    #ratio\n");
	for_each_possible_cpu(cpu)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"cpu%d %10lluus   %3u%%\n",
			cpu, cpu_idle_time(cpu), cpu_idle_ratio(cpu));

	/*
	 * Example of cpu idle state profile result.
	 * Below is an example from the quad core architecture. The number of
	 * rows depends on the number of cpu.
	 *
	 * [state : {desc}]
	 * #cpu   #entry   #cancel      #time    #ratio
	 * cpu0     985        8      8808916us    87%
	 * cpu1     340        2      8311318us    82%
	 * cpu2     270        7      8744801us    87%
	 * cpu3     330        2      9001329us    89%
	 */
	for (i = 0; i < cpu_idle_state_count; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"\n[state : %s]\n", cpu_idle_state[i].desc);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"#cpu   #entry   #cancel      #time    #ratio\n");
		for_each_possible_cpu(cpu) {
			struct cpuidle_stats *stats = &cpu_idle_state[i].stats[cpu];

			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cpu%d   %5u    %5u   %10lluus   %3u%%\n",
				cpu,
				stats->entry_count,
				stats->cancel_count,
				stats->time,
				calculate_percent(stats->time));
		}
	}

	/*
	 * Example of group idle state profile result.
	 * The number of results depends on the number of group idle state.
	 *
	 * [state : {desc}]
	 * #entry   #cancel      #time    #ratio
	 *   52        1       4296397us    42%
	 *
	 * [state : {desc}]
	 * #entry   #cancel      #time    #ratio
	 *    20        0      2230528us    22%
	 */
	for (i = 0; i < group_idle_state_count; i++) {
		struct cpuidle_stats *stats = &group_idle_state[i]->stats;

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"\n[state : %s]\n", group_idle_state[i]->desc);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"#entry   #cancel      #time    #ratio\n");
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"%5u    %5u   %10lluus   %3u%%\n",
			stats->entry_count,
			stats->cancel_count,
			stats->time,
			calculate_percent(stats->time));
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n[IDLE-IP statistics]\n");

	/* fix-idle-ip */
	for (i = 0; i < FIX_IDLE_IP_MAX; i++)
		if (fix_idle_ip_arr[i].count)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"busy IP : %s(count = %d)\n",
					fix_idle_ip_arr[i].name, fix_idle_ip_arr[i].count);
	/* idle-ip */
	list_for_each_entry(ip, &idle_ip_list, list)
		if (ip->count)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"busy IP : %s(count = %d)\n",
					ip->name, ip->count);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#############################################################\n");

	return ret;
}

static void print_result(void)
{
	struct idle_ip *ip;
	int cpu, i;

	if (profile_started) {
		pr_info("CPUIDLE: Profile is ongoing\n");
		return;
	}

	pr_info("#############################################################\n");
	pr_info("Profiling Time : %lluus\n", profile_time);

	/* total idle ratio */
	pr_info("\n[total idle ratio]\n");
	pr_info("#cpu      #time    #ratio\n");
	for_each_possible_cpu(cpu)
		pr_info("cpu%d %10lluus   %3u%%\n",
				cpu, cpu_idle_time(cpu), cpu_idle_ratio(cpu));

	/* cpu idle state */
	for (i = 0; i < cpu_idle_state_count; i++) {
		pr_info("\n[state : %s]\n", cpu_idle_state[i].desc);
		pr_info("#cpu   #entry   #cancel      #time    #ratio\n");
		for_each_possible_cpu(cpu) {
			struct cpuidle_stats *stats = &cpu_idle_state[i].stats[cpu];

			pr_info("cpu%d   %5u    %5u   %10lluus   %3u%%\n",
					cpu,
					stats->entry_count,
					stats->cancel_count,
					stats->time,
					calculate_percent(stats->time));
		}
	}

	/* group idle state (clsuter or system) */
	for (i = 0; i < group_idle_state_count; i++) {
		struct cpuidle_stats *stats = &group_idle_state[i]->stats;

		pr_info("\n[state : %s]\n", group_idle_state[i]->desc);
		pr_info("#entry   #cancel      #time    #ratio\n");
		pr_info("%5u    %5u   %10lluus   %3u%%\n",
				stats->entry_count,
				stats->cancel_count,
				stats->time,
				calculate_percent(stats->time));
	}

	pr_info("\n[IDLE-IP statistics]\n");

	/* fix-idle-ip */
	for (i = 0; i < FIX_IDLE_IP_MAX; i++)
		if (fix_idle_ip_arr[i].count)
			pr_info("busy IP : %s(count = %d)\n",
					fix_idle_ip_arr[i].name, fix_idle_ip_arr[i].count);
	/* idle-ip */
	list_for_each_entry(ip, &idle_ip_list, list)
		if (ip->count)
			pr_info("busy IP : %s(count = %d)\n",
					ip->name, ip->count);

	pr_info("#############################################################\n");
}

/*********************************************************************
 *                          Sysfs interface                          *
 *********************************************************************/
static ssize_t show_cpuidle_profile(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	if (profile_started)
		return snprintf(buf, PAGE_SIZE, "CPUIDLE: Profile is ongoing\n");
	else
		return show_result(dev, attr, buf);
}

static ssize_t store_cpuidle_profile(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	if (!!input)
		cpuidle_profile_start();
	else
		cpuidle_profile_stop();

	return count;
}
static ssize_t show_cpuidle_auto_profile(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{

	if (profile_started)
		return snprintf(buf, PAGE_SIZE, "CPUIDLE: Auto profile is ongoing\n");
	else
		return snprintf(buf, PAGE_SIZE, "CPUIDLE: Auto profile is done\n");
}

static ssize_t store_cpuidle_auto_profile(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int timeout;

	if (!sscanf(buf, "%u", &timeout))
		return -EINVAL;

	if (timeout)
		cpuidle_auto_profile_start(timeout);
	else
		cpuidle_auto_profile_stop();

	return count;
}


static DEVICE_ATTR(profile, 0644, show_cpuidle_profile, store_cpuidle_profile);
static DEVICE_ATTR(auto_profile, 0644, show_cpuidle_auto_profile, store_cpuidle_auto_profile);

static struct attribute *cpuidle_profile_attrs[] = {
	&dev_attr_profile.attr,
	&dev_attr_auto_profile.attr,
	NULL,
};

static const struct attribute_group cpuidle_profile_group = {
	.attrs = cpuidle_profile_attrs,
};

/*********************************************************************
 *                   Initialize cpuidle profiler                     *
 *********************************************************************/
void __init
cpuidle_profile_cpu_idle_register(struct cpuidle_driver *drv)
{
	struct cpu_idle_state *state;
	int state_count = drv->state_count;
	int i;

	state = kzalloc(sizeof(struct cpu_idle_state) * state_count,
							GFP_KERNEL);
	if (!state) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return;
	}

	for (i = 0; i < state_count; i++)
		strncpy(state[i].desc, drv->states[i].desc, DESC_LEN - 1);

	cpu_idle_state = state;
	cpu_idle_state_count = state_count;

	exynos_perf_cpu_idle_register(drv);
}

void __init
cpuidle_profile_group_idle_register(int id, const char *name)
{
	struct group_idle_state *state;

	state = kzalloc(sizeof(struct group_idle_state), GFP_KERNEL);
	if (!state) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return;
	}

	state->id = id;
	strncpy(state->desc, name, DESC_LEN - 1);

	group_idle_state[group_idle_state_count] = state;
	group_idle_state_count++;
}

extern struct class *idle_class;

static int __init cpuidle_profile_init(void)
{
	struct device *dev;
	int ret = 0;

	dev = device_create(idle_class, NULL, MKDEV(0, 0), NULL, "cpuidle_profiler");

	ret = sysfs_create_group(&dev->kobj, &cpuidle_profile_group);
	if (ret)
		pr_err("%s: failed to create sysfs group", __func__);
	else
		INIT_DELAYED_WORK(&auto_profile_work, cpuidle_auto_profile_work);

	return ret;
}
late_initcall(cpuidle_profile_init);
