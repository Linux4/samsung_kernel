/*
 * drivers/cpufreq/cpufreq_limit.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *	Minsung Kim <ms925.kim@samsung.com>
 *	Chiwoong Byun <woong.byun@samsung.com>
 *	 - 2014/10/24 Add HMP feature to support HMP
 *	SangYoung Son <hello.son@samsung.com>
 *	 - 2015/08/06 Support MSM8996(same freq table HMP(Gold, Silver))
 *	 - Use opp table for voltage power efficiency
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <soc/qcom/msm_cpu_voltage.h>

#define MIN(a, b)     (((a) < (b)) ? (a) : (b))
#define MAX(a, b)     (((a) > (b)) ? (a) : (b))

/* cpu frequency table from qcom-cpufreq dt parse */
static struct cpufreq_frequency_table *cpuftbl_L;
static struct cpufreq_frequency_table *cpuftbl_b;

/*
 * reduce voltage gap between gold and silver cluster
 * find optimal silver clock from voltage of gold clock
 */
#undef USE_MATCH_CPU_VOL_FREQ
#if defined(USE_MATCH_CPU_VOL_FREQ)
#define USE_MATCH_CPU_VOL_MAX_FREQ	/* max_limit, thermal case */
#undef USE_MATCH_CPU_VOL_MIN_FREQ	/* min_limit, boosting case */
#endif

/* sched boost type from kernel/sched/sched.h */
#define NO_BOOST 0
#define FULL_THROTTLE_BOOST 1
#define CONSERVATIVE_BOOST 2
#define RESTRAINED_BOOST 3

struct cpufreq_limit_handle {
	struct list_head node;
	unsigned long min;
	unsigned long max;
	char label[20];
};

static DEFINE_MUTEX(cpufreq_limit_lock);
static LIST_HEAD(cpufreq_limit_requests);

#ifdef CONFIG_SCHED_HMP
struct cpufreq_limit_hmp {
	unsigned int 		little_cpu_start;
	unsigned int 		little_cpu_end;
	unsigned int 		big_cpu_start;
	unsigned int 		big_cpu_end;
	unsigned long 		big_min_freq;
	unsigned long 		big_max_freq;
	unsigned long 		little_min_freq;
	unsigned long 		little_max_freq;
	unsigned long 		little_min_lock;
	unsigned int		little_divider;
	bool			use_hotplug_out;
	unsigned int		hmp_boost_type;
	unsigned int		hmp_boost_active;
	int			hmp_prev_boost_type;

	/* set limit of max freq limit for guarantee performance */
	unsigned int		little_limit_max_freq;
};

struct cpufreq_limit_hmp hmp_param = {
	.little_cpu_start 	= 0,
	.little_cpu_end		= 3,
	.big_cpu_start 		= 4,
	.big_cpu_end		= 7,
	.little_min_freq	= 150000,	/* devided value, not real clock */
	.little_max_freq	= 950400,	/* devided value, not real clock */
	.big_min_freq		= 979200,
	.big_max_freq		= 2304000,
	.little_min_lock	= 1171200 / 2, /* devide value is little_divider */

	.little_divider 	= 2,
	.use_hotplug_out	= false,

	.hmp_boost_type		= CONSERVATIVE_BOOST,
	.hmp_boost_active	= 0,
	.hmp_prev_boost_type	= 0,

	.little_limit_max_freq	= 1171200,
};

void cpufreq_limit_corectl(int freq)
{
	unsigned int cpu;
	bool bigout = false;

	/* if div is 1, do not core control */
	if (hmp_param.little_divider == 1)
		return;

	if (!hmp_param.use_hotplug_out)
		return;

	if ((freq > -1) && (freq < hmp_param.big_min_freq))
		bigout = true;

	for_each_possible_cpu(cpu) {
		if ((cpu >= hmp_param.big_cpu_start) &&
			(cpu <= hmp_param.big_cpu_end)) {

			if (bigout)
				cpu_down(cpu);
			else
				cpu_up(cpu);
		}
	}
}


/**
 * cpufreq_limit_set_table - cpufreq table from dt via qcom-cpufreq
 */
void cpufreq_limit_set_table(int cpu, struct cpufreq_frequency_table * ftbl)
{
	int i, count;
	unsigned int max_freq = 0;

	if (cpu == hmp_param.big_cpu_start) {
		cpuftbl_b = ftbl;

		/* update max freq of big cluster */
		for (i = 0; cpuftbl_b[i].frequency != CPUFREQ_TABLE_END; i++)
			count = i;

		for (i = count; i >= 0; i--) {
			if (cpuftbl_b[i].frequency > max_freq)
				max_freq = cpuftbl_b[i].frequency;

			if (cpuftbl_b[i].frequency == CPUFREQ_ENTRY_INVALID)
				continue;
		}
		hmp_param.big_max_freq = max_freq;
		pr_info("%s: max freq of big: %lu\n", __func__, hmp_param.big_max_freq);
	} else if (cpu == hmp_param.little_cpu_start) {
		cpuftbl_L = ftbl;
	}
}

/**
 * cpufreq_limit_get_table - fill the cpufreq table to support HMP
 * @buf		a buf that has been requested to fill the cpufreq table
 */
ssize_t cpufreq_limit_get_table(char *buf)
{
	ssize_t len = 0;
	int i, count = 0;
	unsigned int freq;

	/* big cluster table */
	if (!cpuftbl_b)
		goto little;

	for (i = 0; cpuftbl_b[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = count; i >= 0; i--) {
		freq = cpuftbl_b[i].frequency;

		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		if (freq < hmp_param.big_min_freq || freq > hmp_param.big_max_freq)
			continue;

		len += sprintf(buf + len, "%u ", freq);
	}

	/* if div is 1, use only big cluster freq table */
	if (hmp_param.little_divider == 1)
		goto done;

little:
	/* LITTLE cluster table */
	if (!cpuftbl_L)
		goto done;

	for (i = 0; cpuftbl_L[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = count; i >= 0; i--) {
		freq = cpuftbl_L[i].frequency / hmp_param.little_divider;

		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		if (freq < hmp_param.little_min_freq || freq > hmp_param.little_max_freq)
			continue;

		len += sprintf(buf + len, "%u ", freq);
	}

done:
	len--;
	len += sprintf(buf + len, "\n");

	pr_info("%s: %s\n", __func__, buf);

	return len;
}

static inline int is_little(unsigned int cpu)
{
	return cpu >= hmp_param.little_cpu_start &&
			cpu <= hmp_param.little_cpu_end;
}

static inline int is_big(unsigned int cpu)
{
	return cpu >= hmp_param.big_cpu_start &&
			cpu <= hmp_param.big_cpu_end;
}

static int set_little_divider(struct cpufreq_policy *policy, unsigned long *v)
{
	if (is_little(policy->cpu))
		*v /= hmp_param.little_divider;

	return 0;
}

static int cpufreq_limit_hmp_boost(int enable)
{
	unsigned int ret = 0;

	pr_debug("%s: enable=%d, prev_type=%d, type=%d, active=%d\n", __func__,
		enable, hmp_param.hmp_prev_boost_type, hmp_param.hmp_boost_type, hmp_param.hmp_boost_active);

	if (enable) {
		if ((hmp_param.hmp_boost_type && !hmp_param.hmp_boost_active) ||
			(hmp_param.hmp_boost_active && (hmp_param.hmp_boost_type != hmp_param.hmp_prev_boost_type))) {
			hmp_param.hmp_boost_active = enable;

			// Release previous boost type before enable new boost type.
			if(hmp_param.hmp_prev_boost_type != NO_BOOST) {
				ret = sched_set_boost(hmp_param.hmp_prev_boost_type * -1);
				if (ret)
					pr_err("%s: HMP boost enable failed\n", __func__);
			}

			// Set prev boost type
			hmp_param.hmp_prev_boost_type = hmp_param.hmp_boost_type;

			ret = sched_set_boost(CONSERVATIVE_BOOST);
			if (ret)
				pr_err("%s: HMP boost enable failed\n", __func__);
		}
	} else {
		if (hmp_param.hmp_boost_type && hmp_param.hmp_boost_active) {
			hmp_param.hmp_boost_active = 0;
			ret = sched_set_boost(hmp_param.hmp_prev_boost_type * -1);
			hmp_param.hmp_prev_boost_type = NO_BOOST;
			hmp_param.hmp_boost_type = CONSERVATIVE_BOOST;
			if (ret)
				pr_err("%s: HMP boost disable failed\n", __func__);
		}
	}

	return ret;
}

static int cpufreq_limit_adjust_freq(struct cpufreq_policy *policy,
		unsigned long *min, unsigned long *max)
{
	unsigned int hmp_boost_active = 0;

	pr_debug("%s+: cpu=%d, min=%ld, max=%ld\n", __func__, policy->cpu, *min, *max);

	if (is_little(policy->cpu)) {
		if (*min >= hmp_param.big_min_freq)
			*min = hmp_param.little_min_lock * hmp_param.little_divider;
		else
			*min *= hmp_param.little_divider;

		if (*max >= hmp_param.big_min_freq)
			*max = policy->cpuinfo.max_freq;
		else
			*max *= hmp_param.little_divider;
	} else {
		if (*min >= hmp_param.big_min_freq) {
			hmp_boost_active = 1;
			if (*min >= hmp_param.big_max_freq)
				hmp_param.hmp_boost_type = CONSERVATIVE_BOOST;
			else
				hmp_param.hmp_boost_type = CONSERVATIVE_BOOST;
		} else {
			*min = policy->cpuinfo.min_freq;
			hmp_boost_active = 0;
		}

		if (*max >= hmp_param.big_min_freq) {
			pr_debug("%s: big_min_freq=%ld, max=%ld\n", __func__,
				hmp_param.big_min_freq, *max);
		} else {
			*max = policy->cpuinfo.min_freq;
			hmp_boost_active = 0;
		}

		cpufreq_limit_hmp_boost(hmp_boost_active);
	}

	pr_debug("%s-: cpu=%d, min=%ld, max=%ld\n", __func__, policy->cpu, *min, *max);

	return 0;
}
#else
static inline int cpufreq_limit_adjust_freq(struct cpufreq_policy *policy,
		unsigned long *min, unsigned long *max) { return 0; }
static inline int cpufreq_limit_hmp_boost(int enable) { return 0; }
static inline int set_little_divider(struct cpufreq_policy *policy,
		unsigned long *v) { return 0; }
#endif /* CONFIG_SCHED_HMP */

/**
 * cpufreq_limit_get - limit min_freq or max_freq, return cpufreq_limit_handle
 * @min_freq	limit minimum frequency (0: none)
 * @max_freq	limit maximum frequency (0: none)
 * @label	a literal description string of this request
 */
struct cpufreq_limit_handle *cpufreq_limit_get(unsigned long min_freq,
		unsigned long max_freq, char *label)
{
	struct cpufreq_limit_handle *handle;
	int i, found;

	if (max_freq && max_freq < min_freq)
		return ERR_PTR(-EINVAL);

	mutex_lock(&cpufreq_limit_lock);
	found = 0;
	list_for_each_entry(handle, &cpufreq_limit_requests, node) {
		if (!strncmp(handle->label, label, strlen(handle->label))) {
			found = 1;
			pr_debug("%s: %s is found in list\n", __func__, handle->label);
			break;
		}
	}

	if (found) {
		if (handle->min == min_freq
			 && handle->max == max_freq) {
			pr_info("%s: %s same values(%lu,%lu) entered. just return.\n",
				__func__, handle->label, handle->min, handle->max);
			mutex_unlock(&cpufreq_limit_lock);
			return handle;
		}
	}

	if (!found) {
		handle = kzalloc(sizeof(*handle), GFP_KERNEL);
		if (!handle) {
			pr_err("%s: alloc fail for %s .\n", __func__, label);
			mutex_unlock(&cpufreq_limit_lock);
			return ERR_PTR(-ENOMEM);
		}

		if (strlen(label) < sizeof(handle->label))
			strcpy(handle->label, label);
		else
			strncpy(handle->label, label, sizeof(handle->label) - 1);

		list_add_tail(&handle->node, &cpufreq_limit_requests);
	}

	handle->min = min_freq;
	handle->max = max_freq;

	pr_debug("%s: %s,%lu,%lu\n", __func__, handle->label, handle->min,
			handle->max);

	mutex_unlock(&cpufreq_limit_lock);

        /* Re-evaluate policy to trigger adjust notifier for online CPUs */
	get_online_cpus();

	for_each_online_cpu(i)
		cpufreq_update_policy(i);

	put_online_cpus();

	return handle;
}

/**
 * cpufreq_limit_put - release of a limit of min_freq or max_freq, free
 *			a cpufreq_limit_handle
 * @handle	a cpufreq_limit_handle that has been requested
 */
int cpufreq_limit_put(struct cpufreq_limit_handle *handle)
{
	int i;

	if (handle == NULL || IS_ERR(handle))
		return -EINVAL;

	pr_debug("%s: %s,%lu,%lu\n", __func__, handle->label, handle->min,
			handle->max);

	mutex_lock(&cpufreq_limit_lock);
	list_del(&handle->node);
	mutex_unlock(&cpufreq_limit_lock);

	get_online_cpus();
	for_each_online_cpu(i)
		cpufreq_update_policy(i);
	put_online_cpus();

	kfree(handle);
	return 0;
}

static int cpufreq_limit_notifier_policy(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	struct cpufreq_limit_handle *handle;
	unsigned long min = 0, max = ULONG_MAX;
#if defined(USE_MATCH_CPU_VOL_MIN_FREQ)
	unsigned long adjust_min = 0;
#endif
#if defined(USE_MATCH_CPU_VOL_MAX_FREQ)
	unsigned long adjust_max = 0;
#endif


	if (val != CPUFREQ_ADJUST)
		goto done;

	mutex_lock(&cpufreq_limit_lock);
	list_for_each_entry(handle, &cpufreq_limit_requests, node) {
		if (handle->min > min)
			min = handle->min;
		if (handle->max && handle->max < max)
			max = handle->max;
	}

#ifdef CONFIG_SEC_PM
	pr_debug("CPUFREQ(%d): %s: umin=%d,umax=%d\n",
		policy->cpu, __func__, policy->user_policy.min, policy->user_policy.max);

#ifndef CONFIG_SCHED_HMP /* TODO */
	if (policy->user_policy.min > min)
		min = policy->user_policy.min;
	if (policy->user_policy.max && policy->user_policy.max < max)
		max = policy->user_policy.max;
#endif
#endif

	mutex_unlock(&cpufreq_limit_lock);

#if defined(USE_MATCH_CPU_VOL_FREQ)
	if (is_little(policy->cpu)) {
#if defined(USE_MATCH_CPU_VOL_MIN_FREQ)
		/* boost case */
		if (min)
			adjust_min = msm_match_cpu_voltage_btol(min);

		if (adjust_min)
			min = adjust_min;
#endif
#if defined(USE_MATCH_CPU_VOL_MAX_FREQ)
		/* thermal case */
		if (max && (max < hmp_param.little_max_freq) &&
			(max >= hmp_param.little_limit_max_freq)) {
			adjust_max = msm_match_cpu_voltage_btol(max);

			if (adjust_max)
				max = MAX(adjust_max,
					hmp_param.little_limit_max_freq);
		}
#endif
	}
#endif

	if (!min && max == ULONG_MAX) {
		cpufreq_limit_hmp_boost(0);
		goto done;
	}

	if (!min) {
		min = policy->cpuinfo.min_freq;
		set_little_divider(policy, &min);
	}
	if (max == ULONG_MAX) {
		max = policy->cpuinfo.max_freq;
		set_little_divider(policy, &max);
	}

	/*
	 * little_divider 1 means,
	 * little and big has similar power and performance case
	 * e.g. MSM8996 silver and gold
	 */
	if (hmp_param.little_divider == 1) {
		if (is_big(policy->cpu)) {
			/* sched_boost scenario */
			if (min > hmp_param.little_max_freq)
				cpufreq_limit_hmp_boost(1);
			else
				cpufreq_limit_hmp_boost(0);
		}
	} else {
		cpufreq_limit_adjust_freq(policy, &min, &max);
	}

	pr_debug("%s: limiting cpu%d cpufreq to %lu-%lu\n", __func__,
			policy->cpu, min, max);

	cpufreq_verify_within_limits(policy, min, max);

	pr_debug("%s: limited cpu%d cpufreq to %u-%u\n", __func__,
			policy->cpu, policy->min, policy->max);
done:
	return 0;
}

static struct notifier_block notifier_policy_block = {
	.notifier_call = cpufreq_limit_notifier_policy,
	.priority = -1,
};

/************************** sysfs begin ************************/
static ssize_t show_cpufreq_limit_requests(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct cpufreq_limit_handle *handle;
	ssize_t len = 0;

	mutex_lock(&cpufreq_limit_lock);
	list_for_each_entry(handle, &cpufreq_limit_requests, node) {
		len += sprintf(buf + len, "%s\t%lu\t%lu\n", handle->label,
				handle->min, handle->max);
	}
	mutex_unlock(&cpufreq_limit_lock);

	return len;
}

static struct global_attr cpufreq_limit_requests_attr =
	__ATTR(cpufreq_limit_requests, 0444, show_cpufreq_limit_requests, NULL);

#ifdef CONFIG_SCHED_HMP
#define MAX_ATTRIBUTE_NUM 12

#define show_one(file_name, object)						\
static ssize_t show_##file_name							\
(struct kobject *kobj, struct attribute *attr, char *buf)			\
{										\
	return sprintf(buf, "%u\n", hmp_param.object);				\
}

#define show_one_ulong(file_name, object)					\
static ssize_t show_##file_name							\
(struct kobject *kobj, struct attribute *attr, char *buf)			\
{										\
	return sprintf(buf, "%lu\n", hmp_param.object);			\
}

#define store_one(file_name, object)						\
static ssize_t store_##file_name						\
(struct kobject *a, struct attribute *b, const char *buf, size_t count)		\
{										\
	int ret;								\
										\
	ret = sscanf(buf, "%lu", &hmp_param.object);				\
	if (ret != 1)								\
		return -EINVAL;							\
										\
	return count;								\
}

static ssize_t show_little_cpu_num(struct kobject *kobj, struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u-%u\n", hmp_param.little_cpu_start, hmp_param.little_cpu_end);
}

static ssize_t show_big_cpu_num(struct kobject *kobj, struct attribute *attr, char *buf)
{
	return sprintf(buf, "%u-%u\n", hmp_param.big_cpu_start, hmp_param.big_cpu_end);
}

show_one_ulong(big_min_freq, big_min_freq);
show_one_ulong(big_max_freq, big_max_freq);
show_one_ulong(little_min_freq, little_min_freq);
show_one_ulong(little_max_freq, little_max_freq);
show_one_ulong(little_min_lock, little_min_lock);
show_one(little_divider, little_divider);
show_one(hmp_boost_type, hmp_boost_type);
show_one(hmp_prev_boost_type, hmp_prev_boost_type);

static ssize_t store_little_cpu_num(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	unsigned int input, input2;
	int ret;

	ret = sscanf(buf, "%u-%u", &input, &input2);
	if (ret != 2)
		return -EINVAL;

	if (input >= MAX_ATTRIBUTE_NUM || input2 >= MAX_ATTRIBUTE_NUM)
		return -EINVAL;

	pr_info("%s: %u-%u, ret=%d\n", __func__, input, input2, ret);

	hmp_param.little_cpu_start = input;
	hmp_param.little_cpu_end = input2;

	return count;
}

static ssize_t store_big_cpu_num(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	unsigned int input, input2;
	int ret;

	ret = sscanf(buf, "%u-%u", &input, &input2);
	if (ret != 2)
		return -EINVAL;

	if (input >= MAX_ATTRIBUTE_NUM || input2 >= MAX_ATTRIBUTE_NUM)
		return -EINVAL;

	pr_info("%s: %u-%u, ret=%d\n", __func__, input, input2, ret);

	hmp_param.big_cpu_start = input;
	hmp_param.big_cpu_end = input2;

	return count;
}

store_one(big_min_freq, big_min_freq);
store_one(big_max_freq, big_max_freq);
store_one(little_min_freq, little_min_freq);
store_one(little_max_freq, little_max_freq);
store_one(little_min_lock, little_min_lock);

static ssize_t store_little_divider(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input >= MAX_ATTRIBUTE_NUM)
		return -EINVAL;

	hmp_param.little_divider = input;

	return count;
}

static ssize_t store_hmp_boost_type(struct kobject *a, struct attribute *b,
				const char *buf, size_t count)
{
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 2)
		return -EINVAL;

	hmp_param.hmp_boost_type = input;

	return count;
}

define_one_global_rw(little_cpu_num);
define_one_global_rw(big_cpu_num);
define_one_global_rw(big_min_freq);
define_one_global_rw(big_max_freq);
define_one_global_rw(little_min_freq);
define_one_global_rw(little_max_freq);
define_one_global_rw(little_min_lock);
define_one_global_rw(little_divider);
define_one_global_rw(hmp_boost_type);
define_one_global_ro(hmp_prev_boost_type);
#endif /* CONFIG_SCHED_HMP */

static struct attribute *limit_attributes[] = {
	&cpufreq_limit_requests_attr.attr,
#ifdef CONFIG_SCHED_HMP
	&little_cpu_num.attr,
	&big_cpu_num.attr,
	&big_min_freq.attr,
	&big_max_freq.attr,
	&little_min_freq.attr,
	&little_max_freq.attr,
	&little_min_lock.attr,
	&little_divider.attr,
	&hmp_boost_type.attr,
	&hmp_prev_boost_type.attr,
#endif
	NULL,
};

static struct attribute_group limit_attr_group = {
	.attrs = limit_attributes,
	.name = "cpufreq_limit",
};
/************************** sysfs end ************************/

static int __init cpufreq_limit_init(void)
{
	int ret;

	ret = cpufreq_register_notifier(&notifier_policy_block,
				CPUFREQ_POLICY_NOTIFIER);
	if (ret)
		return ret;

	if (cpufreq_global_kobject) {
		pr_info("%s: sysfs_create_group\n", __func__);
		ret = sysfs_create_group(cpufreq_global_kobject,
				&limit_attr_group);
		if (ret)
			pr_err("%s: failed\n", __func__);
	}

	return ret;
}

static void __exit cpufreq_limit_exit(void)
{
	cpufreq_unregister_notifier(&notifier_policy_block,
			CPUFREQ_POLICY_NOTIFIER);

	sysfs_remove_group(cpufreq_global_kobject, &limit_attr_group);
}

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("'cpufreq_limit' - A driver to limit cpu frequency");
MODULE_LICENSE("GPL");

module_init(cpufreq_limit_init);
module_exit(cpufreq_limit_exit);
