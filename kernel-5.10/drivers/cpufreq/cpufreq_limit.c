/* driver/cpufreq/cpufreq_limit.c
 *
 * CPU limit driver for MTK
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *
 * Author: Sangyoung Son <hello.son@samsung.com>
 *
 * Change limit driver code for MTK chipset by Jonghyeon Cho
 * Author: Jonghyeon Cho <jongjaaa.cho@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/cpufreq.h>
#include <linux/cpufreq_limit.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#endif

#define MAX_BUF_SIZE 1024

static struct freq_qos_request *max_req[DVFS_MAX_ID];
static struct freq_qos_request *min_req[DVFS_MAX_ID];
static struct kobject *cpufreq_kobj;

/* cpu frequency table from cpufreq dt parse */
static struct cpufreq_frequency_table *cpuftbl_ltl;
static struct cpufreq_frequency_table *cpuftbl_big;

static int ltl_max_freq_div;
static int last_min_req_val;
static int last_max_req_val;
DEFINE_MUTEX(cpufreq_limit_mutex);

/*
 * The default value will be updated
 * from device tree in the future.
 */
static struct cpufreq_limit_parameter param = {
	.num_cpu				= 8,
	.freq_count				= 0,
	.ltl_cpu_start			= 0,
	.big_cpu_start			= 6,
	.ltl_max_freq			= 0,
	.ltl_min_lock_freq		= 1100000,
	.big_max_lock_freq		= 750000,
	.ltl_divider			= 4,
	.over_limit				= 0,
};

/**
 * cpufreq_limit_unify_table - make virtual table for limit driver
 */
static void cpufreq_limit_unify_table(void)
{
	unsigned int freq;
	unsigned int count = 0;
	unsigned int freq_count = 0;
	unsigned int ltl_max_freq = 0;
	unsigned int i;

	for (i = 0; cpuftbl_big[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = 0; i < count + 1; i++) {
		freq = cpuftbl_big[i].frequency;

		if (freq == CPUFREQ_ENTRY_INVALID) {
			pr_info("%s: invalid entry in big freq", __func__);
			continue;
		}

		param.unified_table[freq_count++] = freq;
	}

	for (i = 0; cpuftbl_ltl[i].frequency != CPUFREQ_TABLE_END; i++)
		count = i;

	for (i = 0; i < count + 1; i++) {
		freq = cpuftbl_ltl[i].frequency;

		if (freq == CPUFREQ_ENTRY_INVALID) {
			pr_info("%s: invalid entry in ltl freq", __func__);
			continue;
		}

		if (ltl_max_freq < freq)
			ltl_max_freq = freq;

		param.unified_table[freq_count++] = (int)(freq / param.ltl_divider);
	}

	/* update parameters */
	param.ltl_max_freq = ltl_max_freq;
	param.freq_count = freq_count;
	last_min_req_val = -1;
	last_max_req_val = -1;

	ltl_max_freq_div = (int)(ltl_max_freq / param.ltl_divider);
}

/**
 * cpufreq_limit_set_table - little, big frequency table setting
 */
static void cpufreq_limit_set_table(int cpu,
		struct cpufreq_frequency_table *table)
{
	if (cpu == param.big_cpu_start)
		cpuftbl_big = table;
	else if (cpu == param.ltl_cpu_start)
		cpuftbl_ltl = table;

	/*
	 * if the freq table is configured,
	 * start producing the unified frequency table.
	 */
	if (!cpuftbl_ltl || !cpuftbl_big)
		return;

	pr_info("%s: cpufreq table is ready\n", __func__);

	cpufreq_limit_unify_table();
}

/**
 * _set_freq_limit - core function to request frequencies
 * @id			request id
 * @min_req		frequency value to request min lock
 * @max_req		frequency value to request max lock
 */
static int _set_freq_limit(unsigned int id, int min_freq, int max_freq)
{
	int i = 0;

	mutex_lock(&cpufreq_limit_mutex);

	if (min_freq == -1) {
		pr_info("%s: id(%u) min_lock_freq(%d)\n", __func__, id, min_freq);
		for_each_possible_cpu(i)
			freq_qos_update_request(&min_req[id][i],
					FREQ_QOS_MIN_DEFAULT_VALUE);
	} else if (min_freq > 0) {
		pr_info("%s: id(%u) min_lock_freq(%d)\n", __func__, id, min_freq);

		if (min_freq > ltl_max_freq_div) {
			freq_qos_update_request(&min_req[id][param.ltl_cpu_start],
							param.ltl_min_lock_freq);
			freq_qos_update_request(&min_req[id][param.big_cpu_start],
							min_freq);
		} else {
			freq_qos_update_request(&min_req[id][param.ltl_cpu_start],
							min_freq * param.ltl_divider);
		}
	}

	if (max_freq == -1) {
		pr_info("%s: id(%u) max_lock_freq(%d)\n", __func__, id, max_freq);
		for_each_possible_cpu(i)
			freq_qos_update_request(&max_req[id][i],
					FREQ_QOS_MAX_DEFAULT_VALUE);
	} else if (max_freq > 0) {
		pr_info("%s: id(%u) max_lock_freq(%d)\n", __func__, id, max_freq);

		if (max_freq > ltl_max_freq_div) {
			freq_qos_update_request(&max_req[id][param.big_cpu_start],
							max_freq);
		} else {
			freq_qos_update_request(&max_req[id][param.ltl_cpu_start],
							max_freq * param.ltl_divider);
			freq_qos_update_request(&max_req[id][param.big_cpu_start],
							param.big_max_lock_freq);
		}
	}

	mutex_unlock(&cpufreq_limit_mutex);

	return 0;
}

/**
 * set_freq_limit - API provided for frequency min lock operation
 * @id		request id
 * @freq	value to change frequency
 */
int set_freq_limit(unsigned int id, unsigned int freq)
{
	_set_freq_limit(id, freq, 0);
	return 0;
}
EXPORT_SYMBOL(set_freq_limit);

/**
 * cpufreq_limit_get_table - fill the cpufreq table to support HMP
 * @buf		a buf that has been requested to fill the cpufreq table
 */
static ssize_t cpufreq_limit_get_table(char *buf)
{
	ssize_t len = 0;
	int i;

	if (!param.freq_count)
		return len;

	for (i = 0; i < param.freq_count; i++)
		len += snprintf(buf + len, MAX_BUF_SIZE, "%u ",
					param.unified_table[i]);

	len--;
	len += snprintf(buf + len, MAX_BUF_SIZE, "\n");

	return len;
}

/******************************/
/*  Kernel object functions   */
/******************************/

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
		.mode = 0444,							\
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
	return sprintf(buf, "%d\n", last_max_req_val);
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

	_set_freq_limit(DVFS_USER_ID, 0, val);
	last_max_req_val = val;
	ret = count;
out:
	return ret;
}

static ssize_t cpufreq_min_limit_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", last_min_req_val);
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

	_set_freq_limit(DVFS_USER_ID, val, 0);
	last_min_req_val = val;
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

	param.over_limit = (unsigned int)val;
	_set_freq_limit(DVFS_OVERLIMIT_ID, 0, val);
	ret = count;
out:
	return ret;
}

cpufreq_limit_attr_ro(cpufreq_table);
cpufreq_limit_attr(cpufreq_max_limit);
cpufreq_limit_attr(cpufreq_min_limit);
cpufreq_limit_attr(over_limit);

static struct attribute *cpufreq_limit_attr_g[] = {
	&cpufreq_table_attr.attr,
	&cpufreq_max_limit_attr.attr,
	&cpufreq_min_limit_attr.attr,
	&over_limit_attr.attr,
	NULL,
};

static const struct attribute_group cpufreq_limit_attr_group = {
	.attrs = cpufreq_limit_attr_g,
};

#define show_one(_name, object)										\
static ssize_t _name##_show											\
	(struct kobject *kobj, struct kobj_attribute *attr, char *buf)	\
{																	\
	return sprintf(buf, "%u\n", param.object);						\
}

show_one(ltl_cpu_start, ltl_cpu_start);
show_one(big_cpu_start, big_cpu_start);
show_one(ltl_max_freq, ltl_max_freq);
show_one(ltl_min_lock_freq, ltl_min_lock_freq);
show_one(big_max_lock_freq, big_max_lock_freq);
show_one(ltl_divider, ltl_divider);

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

cpufreq_limit_attr_ro(ltl_cpu_start);
cpufreq_limit_attr_ro(big_cpu_start);
cpufreq_limit_attr(ltl_max_freq);
cpufreq_limit_attr(ltl_min_lock_freq);
cpufreq_limit_attr(big_max_lock_freq);
cpufreq_limit_attr(ltl_divider);

static struct attribute *limit_param_attributes[] = {
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
	.name = "parameters",
};

static int cpufreq_limit_add_qos(void)
{
	struct cpufreq_policy *policy;
	unsigned int j;
	unsigned int i = 0;
	int ret = 0;

	for_each_possible_cpu(i) {
		policy = cpufreq_cpu_get(i);
		if (!policy) {
			pr_err("%s: no policy for cpu %d\n", __func__, i);
			ret = -EPROBE_DEFER;
			break;
		}

		for (j = 0; j < DVFS_MAX_ID; j++) {
			ret = freq_qos_add_request(&policy->constraints,
							&min_req[j][i],
							FREQ_QOS_MIN, policy->cpuinfo.min_freq);
			if (ret < 0) {
				pr_err("%s: no policy for cpu %d\n", __func__, i);
				break;
			}

			ret = freq_qos_add_request(&policy->constraints,
							&max_req[j][i],
							FREQ_QOS_MAX, policy->cpuinfo.max_freq);
			if (ret < 0) {
				pr_err("%s: no policy for cpu %d\n", __func__, i);
				break;
			}
		}
		if (ret < 0) {
			cpufreq_cpu_put(policy);
			break;
		}

		/*
		 * Set cpufreq table.
		 * Create a virtual table when internal condition are met.
		 */
		cpufreq_limit_set_table(policy->cpu, policy->freq_table);

		cpufreq_cpu_put(policy);
	}

	return ret;
}

static int cpufreq_limit_req_table_alloc(void)
{
	int i;

	for (i = 0; i < DVFS_MAX_ID; i++) {
		max_req[i] = kcalloc(param.num_cpu, sizeof(struct freq_qos_request),
							GFP_KERNEL);
		min_req[i] = kcalloc(param.num_cpu, sizeof(struct freq_qos_request),
							GFP_KERNEL);

		if (!max_req[i] || !min_req[i])
			return -ENOMEM;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static int cpufreq_limit_parse_dt(struct device_node *np)
{
	int ret;
	u32 val;

	if (!np) {
		pr_info("%s: no device tree\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "num_cpu", &val);
	if (ret)
		return -EINVAL;
	param.num_cpu = val;

	ret = of_property_read_u32(np, "ltl_cpu_start", &val);
	if (ret)
		return -EINVAL;
	param.ltl_cpu_start = val;

	ret = of_property_read_u32(np, "big_cpu_start", &val);
	if (ret)
		return -EINVAL;
	param.big_cpu_start = val;

	ret = of_property_read_u32(np, "ltl_min_lock_freq", &val);
	if (ret)
		return -EINVAL;
	param.ltl_min_lock_freq = val;

	ret = of_property_read_u32(np, "big_max_lock_freq", &val);
	if (ret)
		return -EINVAL;
	param.big_max_lock_freq = val;

	ret = of_property_read_u32(np, "ltl_divider", &val);
	if (ret)
		return -EINVAL;
	param.ltl_divider = val;

	return ret;
}
#endif

int cpufreq_limit_probe(struct platform_device *pdev)
{
	int ret;

	pr_info("%s: start\n", __func__);

#if IS_ENABLED(CONFIG_OF)
	ret = cpufreq_limit_parse_dt(pdev->dev.of_node);
	if (ret < 0) {
		pr_err("%s: fail to parsing device tree\n", __func__);
		goto probe_failed;
	}
#endif
	ret = cpufreq_limit_req_table_alloc();
	if (ret < 0) {
		pr_err("%s: fail to allocation memory\n", __func__);
		goto probe_failed;
	}

	/* Add QoS request */
	ret = cpufreq_limit_add_qos();
	if (ret) {
		pr_err("%s: add qos failed %d\n", __func__, ret);
		goto probe_failed;
	}

	cpufreq_kobj = kobject_create_and_add("cpufreq_limit",
						&cpu_subsys.dev_root->kobj);

	if (!cpufreq_kobj) {
		pr_err("%s: fail to create cpufreq kobj\n", __func__);
		ret = -EAGAIN;
		goto probe_failed;
	}

	ret = sysfs_create_group(cpufreq_kobj, &cpufreq_limit_attr_group);
	if (ret) {
		pr_err("%s: create cpufreq object failed %d\n", __func__, ret);
		goto probe_failed;
	}

	ret = sysfs_create_group(cpufreq_kobj, &limit_param_attr_group);
	if (ret) {
		pr_err("%s: create cpufreq param object failed %d\n", __func__, ret);
		goto probe_failed;
	}

	pr_info("%s: done\n", __func__);

probe_failed:
	return ret;
}

static int cpufreq_limit_remove(struct platform_device *pdev)
{
	int i = 0, j;
	int ret = 0;

	pr_info("%s: start\n", __func__);

	/* remove kernel object */
	if (cpufreq_kobj) {
		sysfs_remove_group(cpufreq_kobj, &limit_param_attr_group);
		sysfs_remove_group(cpufreq_kobj, &cpufreq_limit_attr_group);
		kobject_put(cpufreq_kobj);
		cpufreq_kobj = NULL;
	}

	/* remove qos request */
	for_each_possible_cpu(i) {
		for (j = 0; j < DVFS_MAX_ID; j++) {
			ret = freq_qos_remove_request(&min_req[j][i]);
			if (ret < 0)
				pr_err("%s: failed to remove min_req %d\n", __func__, ret);

			ret = freq_qos_remove_request(&max_req[j][i]);
			if (ret < 0)
				pr_err("%s: failed to remove max_req %d\n", __func__, ret);
		}
	}

	/* deallocation memory */
	for (i = 0; i < DVFS_MAX_ID; i++) {
		kfree(min_req[i]);
		kfree(max_req[i]);
	}

	pr_info("%s: done\n", __func__);
	return ret;
}

static const struct of_device_id cpufreq_limit_match_table[] = {
	{ .compatible = "cpufreq_limit" },
	{}
};

static struct platform_driver cpufreq_limit_driver = {
	.driver = {
		.name = "cpufreq_limit",
		.of_match_table = cpufreq_limit_match_table,
	},
	.probe = cpufreq_limit_probe,
	.remove = cpufreq_limit_remove,
};

static int __init cpufreq_limit_init(void)
{
	return platform_driver_register(&cpufreq_limit_driver);
}

static void __exit cpufreq_limit_exit(void)
{
	platform_driver_unregister(&cpufreq_limit_driver);
}

MODULE_AUTHOR("Sangyoung Son <hello.son@samsung.com>");
MODULE_AUTHOR("Jonghyeon Cho <jongjaaa.cho@samsung.com>");
MODULE_DESCRIPTION("A driver to limit cpu frequency");
MODULE_LICENSE("GPL");

late_initcall(cpufreq_limit_init);
module_exit(cpufreq_limit_exit);
