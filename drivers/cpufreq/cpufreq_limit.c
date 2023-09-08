/*
 * drivers/cpufreq/cpufreq_limit.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
#include <cpu_ctrl.h>
#include <sched_ctl.h>

#define CLUSTER_NUM 2
#define LITTLE 0
#define BIG 1

#define SCHED_BOOST_ENABLE 1

/* cpu frequency table from cpufreq dt parse */
static struct cpufreq_frequency_table *cpuftbl_L;
static struct cpufreq_frequency_table *cpuftbl_b;

static int ltl_max_freq_div;
static struct ppm_limit_data *freq_to_set[DVFS_MAX_ID];
static unsigned int sched_boost_state[DVFS_MAX_ID];
DEFINE_MUTEX(cpufreq_limit_mutex);

static void cpufreq_limit_set_sched_boost(int id, unsigned int val);

struct cpufreq_limit_parameter {
	unsigned int ltl_cpu_start;
	unsigned int big_cpu_start;
	unsigned int ltl_max_freq;
	unsigned int ltl_min_lock_freq;
	unsigned int big_max_lock_freq;
	unsigned int ltl_divider;
	unsigned int over_limit;
};

#if defined(CONFIG_MACH_MT6853)
static struct cpufreq_limit_parameter param = {
	.ltl_cpu_start			= 0,
	.big_cpu_start			= 6,
	.ltl_max_freq			= 2000000,
	.ltl_min_lock_freq		= 1128000,
	.big_max_lock_freq		= 725000,
	.ltl_divider			= 4,
	.over_limit				= -1,
};
#elif defined(CONFIG_MACH_MT6833)
static struct cpufreq_limit_parameter param = {
	.ltl_cpu_start			= 0,
	.big_cpu_start			= 6,
	.ltl_max_freq			= 2000000,
	.ltl_min_lock_freq		= 1115000,
	.big_max_lock_freq		= 725000,
	.ltl_divider			= 4,
	.over_limit				= -1,
};
#elif defined(CONFIG_MACH_MT6877)
static struct cpufreq_limit_parameter param = {
	.ltl_cpu_start			= 0,
	.big_cpu_start			= 6,
	.ltl_max_freq			= 2000000,
	.ltl_min_lock_freq		= 1115000,
	.big_max_lock_freq		= 740000,
	.ltl_divider			= 4,
	.over_limit				= -1,
};
#else
static struct cpufreq_limit_parameter param = {
	.ltl_cpu_start			= 0,
	.big_cpu_start			= 6,
	.ltl_max_freq			= 1800000,
	.ltl_min_lock_freq		= 1175000,
	.big_max_lock_freq		= 850000,
	.ltl_divider			= 4,
	.over_limit				= -1,
};
#endif

void cpufreq_limit_set_table(int cpu, struct cpufreq_frequency_table *ftbl)
{
	if ( cpu == param.big_cpu_start )
		cpuftbl_b = ftbl;
	else if ( cpu == param.ltl_cpu_start )
		cpuftbl_L = ftbl;
}

int set_freq_limit(unsigned int id, int freq)
{
	pr_debug("%s: id=%u freq=%d\n", __func__, id, freq);

	if(unlikely(!freq_to_set[id])) {
		pr_err("%s: cpufreq_limit driver uninitialization\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&cpufreq_limit_mutex);

	if(freq == -1) {
		freq_to_set[id][LITTLE].min = freq;
		freq_to_set[id][BIG].min = freq;
		cpufreq_limit_set_sched_boost(id, SCHED_NO_BOOST);
	} else if(freq > ltl_max_freq_div) {
		freq_to_set[id][LITTLE].min = param.ltl_min_lock_freq;
		freq_to_set[id][BIG].min = freq;
#ifdef CONFIG_PRIO_PINNED_BOOST
		cpufreq_limit_set_sched_boost(id, SCHED_PINNED_BOOST);
#else
		cpufreq_limit_set_sched_boost(id, SCHED_ALL_BOOST);
#endif
	} else {
		freq_to_set[id][LITTLE].min = freq * param.ltl_divider;
		freq_to_set[id][BIG].min = -1;
		cpufreq_limit_set_sched_boost(id, SCHED_NO_BOOST);
	}

	switch(id) {
	case DVFS_USER_ID:
		update_userlimit_cpu_freq(CPU_KIR_SEC_LIMIT, CLUSTER_NUM, freq_to_set[id]);
		break;
	case DVFS_TOUCH_ID:
		update_userlimit_cpu_freq(CPU_KIR_SEC_TOUCH, CLUSTER_NUM, freq_to_set[id]);
		break;
	case DVFS_FINGER_ID:
		update_userlimit_cpu_freq(CPU_KIR_SEC_FINGER, CLUSTER_NUM, freq_to_set[id]);
		break;
	}

	mutex_unlock(&cpufreq_limit_mutex);

	return 0;
}

/**
 * cpufreq_limit_get_table - fill the cpufreq table to support HMP
 * @buf		a buf that has been requested to fill the cpufreq table
 */
static ssize_t cpufreq_limit_get_table(char *buf)
{
	ssize_t len = 0;
	int i, k;
	int count_b = 0, count_l = 0;

	if(!cpuftbl_b || !cpuftbl_L) {
		pr_err("%s: Can not find cpufreq table\n", __func__);
		return len;
	}

	for (i = 0; cpuftbl_b[i].frequency != CPUFREQ_TABLE_END; i++){};
	count_b = i;

	for (i = 0; cpuftbl_L[i].frequency != CPUFREQ_TABLE_END; i++){};
	count_l = i;

	for (i = 0, k = 0; i < count_b && k < count_l; ) {
		if(cpuftbl_b[i].frequency > (unsigned int)(cpuftbl_L[k].frequency / param.ltl_divider))
			len += sprintf(buf + len,"%u ", cpuftbl_b[i++].frequency);
		else if(cpuftbl_b[i].frequency < (unsigned int)(cpuftbl_L[k].frequency / param.ltl_divider))
			len += sprintf(buf + len,"%u ", (unsigned int)(cpuftbl_L[k++].frequency / param.ltl_divider));
		else{
			len += sprintf(buf + len,"%u ", cpuftbl_b[i].frequency);
			i++;
			k++;
		}
	}

	while(i < count_b)
		len += sprintf(buf + len,"%u ", cpuftbl_b[i++].frequency);

	while(k < count_l)
		len += sprintf(buf + len,"%u ", (unsigned int)(cpuftbl_L[k++].frequency / param.ltl_divider));

	len = (len != 0) ? len - 1 : len;
	len += sprintf(buf + len, "\n");

	pr_info("%s: %s", __func__, buf);

	return len;
}

#if SCHED_BOOST_ENABLE
static void cpufreq_limit_set_sched_boost(int id, unsigned int val)
{
	int i;
	int boost_val = SCHED_NO_BOOST;

	if (val >= SCHED_UNKNOWN_BOOST || val < SCHED_NO_BOOST)
		return;

	sched_boost_state[id] = val;

	for(i = 0; i < DVFS_MAX_ID; i++) {
		if (sched_boost_state[i] > boost_val)
			boost_val = sched_boost_state[i];
	}

	set_sched_boost(boost_val);
}
#else
static void cpufreq_limit_set_sched_boost(int id, unsigned int val) {}
#endif

#define MAX(x,y) (((x) > (y) ? (x) : (y)))
#define MIN(x,y) (((x) < (y) ? (x) : (y)))

#define cpufreq_limit_attr(_name)				\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {									\
		.name = __stringify(_name),				\
		.mode = 0644,							\
	},											\
	.show	= _name##_show,						\
	.store	= _name##_store,					\
}

#define cpufreq_limit_attr_ro(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {									\
		.name = __stringify(_name),				\
		.mode = S_IRUGO,						\
	},											\
	.show	= _name##_show,						\
}

static ssize_t cpufreq_table_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return cpufreq_limit_get_table(buf);
}

static ssize_t cpufreq_max_limit_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	int i, val = 0xFFFFFFF;

	mutex_lock(&cpufreq_limit_mutex);
	for (i = 0; i < DVFS_MAX_ID; i++) {
		if (i == DVFS_OVERLIMIT_ID)
			continue;

		if(freq_to_set[i][BIG].max != -1)
			val = MIN(freq_to_set[i][BIG].max, val);

		if(freq_to_set[i][LITTLE].max != -1)
			val = MIN((int)(freq_to_set[i][LITTLE].max / param.ltl_divider), val);
	}
	mutex_unlock(&cpufreq_limit_mutex);

	val = val != 0xFFFFFFF ? val : -1;
	return sprintf(buf, "%d\n", val);
}

static ssize_t cpufreq_max_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	mutex_lock(&cpufreq_limit_mutex);
	if(val == -1) {
		freq_to_set[DVFS_USER_ID][LITTLE].max = val;
		freq_to_set[DVFS_USER_ID][BIG].max = val;
	} else if(val > ltl_max_freq_div) {
		freq_to_set[DVFS_USER_ID][LITTLE].max = -1;
		freq_to_set[DVFS_USER_ID][BIG].max = val;
	} else {
		freq_to_set[DVFS_USER_ID][LITTLE].max = val * param.ltl_divider;
		freq_to_set[DVFS_USER_ID][BIG].max = param.big_max_lock_freq;
	}

	update_userlimit_cpu_freq(CPU_KIR_SEC_LIMIT, CLUSTER_NUM, freq_to_set[DVFS_USER_ID]);
	mutex_unlock(&cpufreq_limit_mutex);

	ret = count;
out:
	return ret;
}

static ssize_t cpufreq_min_limit_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	int i, val = -1;

	mutex_lock(&cpufreq_limit_mutex);
	for (i = 0; i < DVFS_MAX_ID; i++) {
		if(freq_to_set[i][BIG].min != -1)
			val = MAX(freq_to_set[i][BIG].min, val);

		if(freq_to_set[i][LITTLE].min != -1)
			val = MAX((int)(freq_to_set[i][LITTLE].min / param.ltl_divider), val);
	}
	mutex_unlock(&cpufreq_limit_mutex);

	return sprintf(buf, "%d\n", val);
}

static ssize_t cpufreq_min_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	set_freq_limit(DVFS_USER_ID, val);
	ret = count;
out:
	return ret;
}

static ssize_t over_limit_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", param.over_limit);
}

static ssize_t over_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val;
	ssize_t ret = -EINVAL;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	mutex_lock(&cpufreq_limit_mutex);
	param.over_limit = (unsigned int)val;
	if(val == -1) {
		freq_to_set[DVFS_OVERLIMIT_ID][LITTLE].max = val;
		freq_to_set[DVFS_OVERLIMIT_ID][BIG].max = val;
	} else if(val > ltl_max_freq_div) {
		freq_to_set[DVFS_OVERLIMIT_ID][LITTLE].max = -1;
		freq_to_set[DVFS_OVERLIMIT_ID][BIG].max = val;
	} else {
		freq_to_set[DVFS_OVERLIMIT_ID][LITTLE].max = val * param.ltl_divider;
		freq_to_set[DVFS_OVERLIMIT_ID][BIG].max = param.big_max_lock_freq;
	}

	update_userlimit_cpu_freq(CPU_KIR_SEC_OVERLIMIT, CLUSTER_NUM, freq_to_set[DVFS_OVERLIMIT_ID]);
	mutex_unlock(&cpufreq_limit_mutex);
	ret = count;
out:
	return ret;
}

cpufreq_limit_attr_ro(cpufreq_table);
cpufreq_limit_attr(cpufreq_max_limit);
cpufreq_limit_attr(cpufreq_min_limit);
cpufreq_limit_attr(over_limit);

static struct attribute * g[] = {
	&cpufreq_table_attr.attr,
	&cpufreq_max_limit_attr.attr,
	&cpufreq_min_limit_attr.attr,
	&over_limit_attr.attr,
	NULL,
};

static const struct attribute_group limit_attr_group = {
	.attrs = g,
};

#define show_one(_name, object)											\
static ssize_t _name##_show												\
	(struct kobject *kobj, struct kobj_attribute *attr, char *buf)		\
{																		\
	return sprintf(buf, "%u\n", param.object);							\
}

show_one(ltl_cpu_start, ltl_cpu_start);
show_one(big_cpu_start, big_cpu_start);
show_one(ltl_max_freq, ltl_max_freq);
show_one(ltl_min_lock_freq, ltl_min_lock_freq);
show_one(big_max_lock_freq, big_max_lock_freq);
show_one(ltl_divider, ltl_divider);

static ssize_t cpufreq_limit_requests_show
	(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;

	mutex_lock(&cpufreq_limit_mutex);
	for (i = 0; i < DVFS_MAX_ID; i++) {
		count += sprintf(buf + count, "ID[%d]: %d %d %d %d\n",
					i, freq_to_set[i][LITTLE].min, freq_to_set[i][BIG].min,
					freq_to_set[i][LITTLE].max, freq_to_set[i][BIG].max);
	}
	mutex_unlock(&cpufreq_limit_mutex);

	return count;
}

static ssize_t ltl_max_freq_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	param.ltl_max_freq = val;
	ltl_max_freq_div = (int)(param.ltl_max_freq / param.ltl_divider);

	ret = count;
out:
	return ret;
}

static ssize_t ltl_min_lock_freq_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	param.ltl_min_lock_freq = val;

	ret = count;
out:
	return ret;
}

static ssize_t big_max_lock_freq_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	param.big_max_lock_freq = val;

	ret = count;
out:
	return ret;
}

static ssize_t ltl_divider_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	param.ltl_divider = val;
	ltl_max_freq_div = (int)(param.ltl_max_freq / param.ltl_divider);

	ret = count;
out:
	return ret;
}

cpufreq_limit_attr_ro(cpufreq_limit_requests);
cpufreq_limit_attr_ro(ltl_cpu_start);
cpufreq_limit_attr_ro(big_cpu_start);
cpufreq_limit_attr(ltl_max_freq);
cpufreq_limit_attr(ltl_min_lock_freq);
cpufreq_limit_attr(big_max_lock_freq);
cpufreq_limit_attr(ltl_divider);

static struct attribute *limit_param_attributes[] = {
	&cpufreq_limit_requests_attr.attr,
	&ltl_cpu_start_attr.attr,
	&big_cpu_start_attr.attr,
	&ltl_max_freq_attr.attr,
	&ltl_min_lock_freq_attr.attr,
	&big_max_lock_freq_attr.attr,
	&ltl_divider_attr.attr,
	NULL,
};

static struct attribute_group limit_param_attr_group = {
	.attrs = limit_param_attributes,
	.name = "cpufreq_limit",
};

static int __init cpufreq_limit_init(void)
{
	int i, ret = 0;

	for (i = 0; i < DVFS_MAX_ID; i++) {
		freq_to_set[i] = kcalloc(CLUSTER_NUM, sizeof(struct ppm_limit_data), GFP_KERNEL);
		if (!freq_to_set[i]) {
			pr_err("%s: failed, kcalloc freq_to_set fail\n", __func__);
			return -ENOMEM;
		}
	}

	for (i = 0; i < DVFS_MAX_ID; i++) {
		freq_to_set[i][LITTLE].min = -1;
		freq_to_set[i][LITTLE].max = -1;
		freq_to_set[i][BIG].min = -1;
		freq_to_set[i][BIG].max = -1;
		sched_boost_state[i] = SCHED_NO_BOOST;
	}

	ltl_max_freq_div = (int)(param.ltl_max_freq / param.ltl_divider);

	if(power_kobj) {
		ret = sysfs_create_group(power_kobj, &limit_attr_group);
		if (ret)
			pr_err("%s: failed %d\n", __func__, ret);
	}

	if (cpufreq_global_kobject) {
		ret = sysfs_create_group(cpufreq_global_kobject, &limit_param_attr_group);
		if (ret)
			pr_err("%s: failed\n", __func__, ret);
	}

	pr_info("%s: cpufreq_limit driver initialization done\n", __func__);

	return ret;
}

static void __exit cpufreq_limit_exit(void)
{
	int i;

	sysfs_remove_group(power_kobj, &limit_param_attr_group);
	sysfs_remove_group(power_kobj, &limit_attr_group);
	for (i = 0; i < DVFS_MAX_ID; i++)
		kfree(freq_to_set[i]);
}

MODULE_DESCRIPTION("A driver to limit cpu frequency");
MODULE_LICENSE("GPL");

module_init(cpufreq_limit_init);
module_exit(cpufreq_limit_exit);
