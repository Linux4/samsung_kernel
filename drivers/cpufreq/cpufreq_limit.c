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

#define CLUSTER_NUM 1

static struct cpufreq_frequency_table *cpuftbl;
static unsigned int over_limit_param;

static struct ppm_limit_data *freq_to_set;
DEFINE_MUTEX(cpufreq_limit_mutex);

void cpufreq_limit_set_table(int cpu, struct cpufreq_frequency_table *ftbl)
{
	cpuftbl = ftbl;
}

int set_freq_limit(unsigned int id, int freq)
{
	int i;
	pr_debug("%s: id=%d freq=%d\n", __func__, (int)id, freq);

	if(unlikely(!freq_to_set)) {
		pr_err("%s: cpufreq_limit driver uninitialized\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&cpufreq_limit_mutex);
	for (i = 0; i < CLUSTER_NUM; i++) {
		freq_to_set[i].min = freq;
	}
	switch(id) {
		case DVFS_TOUCH_ID:
		update_userlimit_cpu_freq(CPU_KIR_SEC_TOUCH, CLUSTER_NUM, freq_to_set);
		break;
		case DVFS_FINGER_ID:
		update_userlimit_cpu_freq(CPU_KIR_SEC_FINGER, CLUSTER_NUM, freq_to_set);
		break;
	}
	mutex_unlock(&cpufreq_limit_mutex);

	return 0;
}

/**
 * For MT6739
 * cpufreq_limit_get_table - fill the cpufreq table to support HMP
 * @buf		a buf that has been requested to fill the cpufreq table
 */
static ssize_t cpufreq_limit_get_table(char *buf)
{
	ssize_t len = 0;
	int i, count = 0;

	if(!cpuftbl) {
		pr_err("%s: Can not find cpufreq table\n", __func__);
		return len;
	}

	for (i = 0; cpuftbl[i].frequency != CPUFREQ_TABLE_END; i++){};
	count = i;

	for (i = 0; i < count; ) {
		len += sprintf(buf + len,"%u ", cpuftbl[i++].frequency);
	}

	len = (len != 0) ? len - 1 : len;
	len += sprintf(buf + len, "\n");

	pr_info("%s: %s", __func__, buf);

	return len;
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
	int val;

	mutex_lock(&cpufreq_limit_mutex);
	val = freq_to_set[0].max;
	mutex_unlock(&cpufreq_limit_mutex);

	return sprintf(buf, "%d\n", val);
}

static ssize_t cpufreq_max_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val, i;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	mutex_lock(&cpufreq_limit_mutex);
	for (i = 0; i < CLUSTER_NUM; i++) {
			freq_to_set[i].max = val;
	}
	update_userlimit_cpu_freq(CPU_KIR_SEC_LIMIT, CLUSTER_NUM, freq_to_set);
	mutex_unlock(&cpufreq_limit_mutex);

	ret = count;
out:
	return ret;
}

static ssize_t cpufreq_min_limit_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	int val;

	mutex_lock(&cpufreq_limit_mutex);
	val = freq_to_set[0].min;
	mutex_unlock(&cpufreq_limit_mutex);

	return sprintf(buf, "%d\n", val);
}

static ssize_t cpufreq_min_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	int val, i;
	ssize_t ret = -EINVAL;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	mutex_lock(&cpufreq_limit_mutex);
	for (i = 0; i < CLUSTER_NUM; i++) {
			freq_to_set[i].min = val;
	}
	update_userlimit_cpu_freq(CPU_KIR_SEC_LIMIT, CLUSTER_NUM, freq_to_set);
	mutex_unlock(&cpufreq_limit_mutex);

	ret = count;
out:
	return ret;
}

void cpufreq_limit_set_over_limit(unsigned int val)
{
	over_limit_param = val;
	update_userlimit_cpu_freq(CPU_KIR_SEC_LIMIT, CLUSTER_NUM, freq_to_set);	
}

unsigned int cpufreq_limit_get_over_limit(void)
{
	return over_limit_param;
}

static ssize_t over_limit_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", cpufreq_limit_get_over_limit());
}

static ssize_t over_limit_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t n)
{
	unsigned int val;
	ssize_t ret = -EINVAL;

	ret = kstrtoint(buf, 10, &val);
	if (ret < 0) {
		pr_err("%s: Invalid cpufreq format\n", __func__);
		goto out;
	}

	mutex_lock(&cpufreq_limit_mutex);
	cpufreq_limit_set_over_limit((unsigned int)val);
	mutex_unlock(&cpufreq_limit_mutex);
	ret = n;
out:
	return ret;
}

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

static int __init cpufreq_limit_init(void)
{
	int ret, i;

	freq_to_set = kcalloc(CLUSTER_NUM, sizeof(struct ppm_limit_data), GFP_KERNEL);
	if (!freq_to_set) {
		pr_err("%s: failed, kcalloc freq_to_set fail\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < CLUSTER_NUM; i++) {
			freq_to_set[i].min = -1;
			freq_to_set[i].max = -1;
	}

	ret = sysfs_create_group(power_kobj, &limit_attr_group);
	if (ret)
		pr_err("%s: failed %d\n", __func__, ret);

	return ret;
}

static void __exit cpufreq_limit_exit(void)
{
	sysfs_remove_group(power_kobj, &limit_attr_group);
	kfree(freq_to_set);
}

MODULE_DESCRIPTION("A driver to limit cpu frequency");
MODULE_LICENSE("GPL");

module_init(cpufreq_limit_init);
module_exit(cpufreq_limit_exit);
