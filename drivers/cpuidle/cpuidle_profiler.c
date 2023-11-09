/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>

#include <asm/page.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#include <mach/exynos-powermode.h>

#include "cpuidle_profiler.h"

static bool profile_ongoing;

static DEFINE_PER_CPU(struct cpuidle_profile_info, profile_info);

/*********************************************************************
 *                         helper function                           *
 *********************************************************************/
#define state_entered(state)	(((int)state < (int)0) ? 0 : 1)

static void profile_start(struct cpuidle_profile_info *info , int state, ktime_t now)
{
	if (state_entered(info->cur_state))
		return;

	info->cur_state = state;
	info->last_entry_time = now;

	info->usage[state].entry_count++;
}

static void count_earlywakeup(struct cpuidle_profile_info *info , int state)
{
	if (!state_entered(info->cur_state))
		return;

	info->cur_state = -EINVAL;
	info->usage[state].early_wakeup_count++;
}

static void update_time(struct cpuidle_profile_info *info , int state, ktime_t now)
{
	s64 diff;

	if (!state_entered(info->cur_state))
		return;

	info->cur_state = -EINVAL;

	diff = ktime_to_us(ktime_sub(now, info->last_entry_time));
	info->usage[state].time += diff;
}

/*********************************************************************
 *                Information gathering function                     *
 *********************************************************************/
void cpuidle_profile_start(int cpu, int state)
{
	struct cpuidle_profile_info *info;
	ktime_t now;

	if (!profile_ongoing)
		return;

	now = ktime_get();
	info = &per_cpu(profile_info, cpu);

	profile_start(info, state, now);
}

void cpuidle_profile_finish(int cpu, int early_wakeup)
{
	struct cpuidle_profile_info *info;
	int state;
	ktime_t now;

	if (!profile_ongoing)
		return;

	info = &per_cpu(profile_info, cpu);
	state = info->cur_state;

	if (early_wakeup) {
		count_earlywakeup(info, state);

		/*
		 * If cpu cannot enter power mode, residency time
		 * should not be updated.
		 */
		return;
	}

	now = ktime_get();
	update_time(info, state, now);
}

/*********************************************************************
 *                            Show result                            *
 *********************************************************************/
static ktime_t profile_start_time;
static ktime_t profile_finish_time;
static s64 profile_time;

int state_count;

static int calculate_percent(s64 residency)
{
	if (!residency)
		return 0;

	residency *= 100;
	do_div(residency, profile_time);

	return residency;
}

static void show_result(void)
{
	int i, cpu;
	struct cpuidle_profile_info *info;

	pr_info("Profiling Time : %lluus\n", profile_time);

	for (i = 0; i < state_count; i++) {
		pr_info("[state%d]\n", i);
		pr_info("#cpu   #entry   #early      #time    #ratio\n");
		for_each_possible_cpu(cpu) {
			info = &per_cpu(profile_info, cpu);
			pr_info("cpu%d   %5u   %5u   %10lluus   %3u%%\n", cpu,
				info->usage[i].entry_count,
				info->usage[i].early_wakeup_count,
				info->usage[i].time,
				calculate_percent(info->usage[i].time));
		}

		pr_info("\n");
	}
}

/*********************************************************************
 *                      Main profile function                        *
 *********************************************************************/
static void clear_profile_state_usage(struct cpuidle_profile_state_usage *usage)
{
	usage->entry_count = 0;
	usage->early_wakeup_count = 0;
	usage->time = 0;
}

static void clear_profile_info(struct cpuidle_profile_info *info, int state_num)
{
	int state;

	for (state = 0; state < state_num; state++)
		clear_profile_state_usage(&info->usage[state]);

	info->cur_state = -EINVAL;
	info->last_entry_time.tv64 = 0;
}

static void clear_time(void)
{
	int i;

	profile_start_time.tv64 = 0;
	profile_finish_time.tv64 = 0;

	for_each_possible_cpu(i)
		clear_profile_info(&per_cpu(profile_info, i), state_count);
}

static void call_cpu_start_profile(void *p) {};
static void call_cpu_finish_profile(void *p) {};

static void cpuidle_profile_main_start(void)
{
	if (profile_ongoing) {
		pr_err("cpuidle profile is ongoing\n");
		return;
	}

	clear_time();
	profile_start_time = ktime_get();

	profile_ongoing = 1;

	/* Wakeup all cpus and clear own profile data to start profile */
	preempt_disable();
	clear_profile_info(&per_cpu(profile_info, smp_processor_id()), state_count);
	smp_call_function(call_cpu_start_profile, NULL, 1);
	preempt_enable();

	pr_info("cpuidle profile start\n");
}

static void cpuidle_profile_main_finish(void)
{
	if (!profile_ongoing) {
		pr_err("CPUIDLE profile does not start yet\n");
		return;
	}

	pr_info("cpuidle profile finish\n");

	/* Wakeup all cpus to update own profile data to finish profile */
	preempt_disable();
	smp_call_function(call_cpu_finish_profile, NULL, 1);
	preempt_enable();

	profile_ongoing = 0;

	profile_finish_time = ktime_get();
	profile_time = ktime_to_us(ktime_sub(profile_finish_time,
						profile_start_time));

	show_result();
}

/*********************************************************************
 *                          Sysfs interface                          *
 *********************************************************************/
static ssize_t show_cpuidle_profile(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;

	if (profile_ongoing)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPUIDLE profile is ongoing\n");
	else
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"echo <1/0> > profile\n");

	return ret;
}

static ssize_t store_cpuidle_profile(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	if (!!input)
		cpuidle_profile_main_start();
	else
		cpuidle_profile_main_finish();

	return count;
}

static struct kobj_attribute cpuidle_profile_attr =
	__ATTR(profile, 0644, show_cpuidle_profile, store_cpuidle_profile);

static struct attribute *cpuidle_profile_attrs[] = {
	&cpuidle_profile_attr.attr,
	NULL,
};

static const struct attribute_group cpuidle_profile_group = {
	.attrs = cpuidle_profile_attrs,
};


/*********************************************************************
 *                   Initialize cpuidle profiler                     *
 *********************************************************************/
static void __init cpuidle_profile_usage_init(void)
{
	int i, size;
	struct cpuidle_profile_info *info;

	if (!state_count) {
		pr_err("%s: cannot get the number of cpuidle state\n", __func__);
		return;
	}

	size = sizeof(struct cpuidle_profile_state_usage) * state_count;
	for_each_possible_cpu(i) {
		info = &per_cpu(profile_info, i);

		info->usage = kmalloc(size, GFP_KERNEL);
		if (!info->usage) {
			pr_err("%s:%d: Memory allocation failed\n", __func__, __LINE__);
			return;
		}
	}
}

void __init cpuidle_profile_state_init(struct cpuidle_driver *drv)
{
	state_count = drv->state_count;

	cpuidle_profile_usage_init();
}

static int __init cpuidle_profile_init(void)
{
	struct class *class;
	struct device *dev;

	class = class_create(THIS_MODULE, "cpuidle");
	dev = device_create(class, NULL, 0, NULL, "cpuidle_profiler");

	if (sysfs_create_group(&dev->kobj, &cpuidle_profile_group)) {
		pr_err("CPUIDLE Profiler : error to create sysfs\n");
		return -EINVAL;
	}

	return 0;
}
late_initcall(cpuidle_profile_init);
