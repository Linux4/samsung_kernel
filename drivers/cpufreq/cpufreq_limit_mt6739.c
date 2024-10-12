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

/* cpu frequency table from cpufreq dt parse */
static struct cpufreq_frequency_table *cpuftbl;
static struct ppm_limit_data *freq_to_set[DVFS_MAX_ID];
DEFINE_MUTEX(cpufreq_limit_mutex);

#define CLUSTER_NUM 1
#define LITTLE 0

struct cpufreq_limit_parameter {
	unsigned int over_limit;
};

static struct cpufreq_limit_parameter param = {
	.over_limit				= -1,
};

void cpufreq_limit_set_table(int cpu, struct cpufreq_frequency_table *ftbl)
{
	if (cpu == 0)
		cpuftbl = ftbl;
}

int set_freq_limit(unsigned int id, int freq)
{
	pr_debug("%s: id=%u freq=%d\n", __func__, id, freq);

	if (unlikely(!freq_to_set[id])) {
		pr_err("%s: cpufreq_limit driver uninitialization\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&cpufreq_limit_mutex);

	freq_to_set[id][LITTLE].min = freq;

	switch (id) {
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

static ssize_t cpufreq_limit_get_table(char *buf)
{
	ssize_t len = 0;
	int i;
	unsigned int prev_freq = 0;

	if (!cpuftbl) {
		pr_err("%s: Can not find cpufreq table\n", __func__);
		return len;
	}

	for (i = 0; cpuftbl[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (prev_freq != cpuftbl[i].frequency)
			len += sprintf(buf + len, "%u ", cpuftbl[i].frequency);

		prev_freq = cpuftbl[i].frequency;
	}

	len = (len != 0) ? len - 1 : len;
	len += sprintf(buf + len, "\n");

	pr_info("%s: %s", __func__, buf);

	return len;
}

#define MAX(x, y) (((x) > (y) ? (x) : (y)))
#define MIN(x, y) (((x) < (y) ? (x) : (y)))

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
		.mode = 0444,						\
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

		if (freq_to_set[i][LITTLE].max != -1)
			val = MIN(freq_to_set[i][LITTLE].max, val);
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

	if (kstrtoint(buf, 10, &val)) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	mutex_lock(&cpufreq_limit_mutex);
	freq_to_set[DVFS_USER_ID][LITTLE].max = val;
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
	for (i = 0; i < DVFS_MAX_ID; i++)
		if (freq_to_set[i][LITTLE].min != -1)
			val = MAX(freq_to_set[i][LITTLE].min, val);
	mutex_unlock(&cpufreq_limit_mutex);

	return sprintf(buf, "%d\n", val);
}

static ssize_t cpufreq_min_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val;
	ssize_t ret = -EINVAL;

	if (kstrtoint(buf, 10, &val)) {
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
	freq_to_set[DVFS_OVERLIMIT_ID][LITTLE].max = val;
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

static struct attribute *g[] = {
	&cpufreq_table_attr.attr,
	&cpufreq_max_limit_attr.attr,
	&cpufreq_min_limit_attr.attr,
	&over_limit_attr.attr,
	NULL,
};

static const struct attribute_group limit_attr_group = {
	.attrs = g,
};

static ssize_t cpufreq_limit_requests_show
	(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;

	mutex_lock(&cpufreq_limit_mutex);
	for (i = 0; i < DVFS_MAX_ID; i++) {
		count += sprintf(buf + count, "ID[%d]: %d %d\n",
					i, freq_to_set[i][LITTLE].min, freq_to_set[i][LITTLE].max);
	}
	mutex_unlock(&cpufreq_limit_mutex);

	return count;
}

cpufreq_limit_attr_ro(cpufreq_limit_requests);

static struct attribute *limit_param_attributes[] = {
	&cpufreq_limit_requests_attr.attr,
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
		if (!freq_to_set[i])
			return -ENOMEM;
	}

	for (i = 0; i < DVFS_MAX_ID; i++) {
		freq_to_set[i][LITTLE].min = -1;
		freq_to_set[i][LITTLE].max = -1;
	}

	if (power_kobj) {
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
