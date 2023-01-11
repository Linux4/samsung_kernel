/* linux/driver/cpuhotplug/cpunum_qos.c
 *
 *  Copyright (C) 2014 Marvell, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/sysfs.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/cpunum_qos.h>

#define NUM_CPUS       num_possible_cpus()
#define NUM_ON_CPUS    num_online_cpus()

/* sync hotplug policy and qos request */
static DEFINE_MUTEX(hotplug_qos_lock);

static struct pm_qos_request cpunum_qos_min = {
	.name = "req_usermin",
};

static struct pm_qos_request cpunum_qos_max = {
	.name = "req_usermax",
};

void cpunum_qos_lock(void)
{
	mutex_lock(&hotplug_qos_lock);
}

void cpunum_qos_unlock(void)
{
	mutex_unlock(&hotplug_qos_lock);
}

static void cpu_plugin_onecore(void)
{
	int i;
	unsigned int target = NR_CPUS;

	get_online_cpus();
	for (i = 0; i < NR_CPUS; i++) {
		if (!cpu_online(i)) {
			target = i;
			break;
		}
	}
	put_online_cpus();

	if ((target >= 0) && (target < NR_CPUS))
		cpu_up(target);
}

static void cpu_plugout_onecore(void)
{
	int i;
	unsigned int target = NR_CPUS;

	get_online_cpus();
	/* not permit CPU0 to plug out */
	for (i = NR_CPUS - 1; i > 0; i--) {
		if (cpu_online(i)) {
			target = i;
			break;
		}
	}
	put_online_cpus();

	if ((target > 0) && (target < NR_CPUS))
		cpu_down(target);
}

static int cpunum_notify(struct notifier_block *b, unsigned long val, void *v)
{
	s32 qos_min = 0;
	s32 qos_max = 0;
	s32 real_min = 0, real_max = 0;
	s32 in_num = 0, out_num = 0;
	int i;

	cpunum_qos_lock();
	qos_min = pm_qos_request(PM_QOS_CPU_NUM_MIN);
	qos_max = pm_qos_request(PM_QOS_CPU_NUM_MAX);
	if (qos_min <= qos_max) {
		real_min = qos_min;
		real_max = qos_max;
	} else {
		/* max has higher priority */
		real_min = real_max = qos_max;
	}

	/* here ganrantee real_min <= real_max */
	if (NUM_ON_CPUS < real_min) {
		in_num = real_min - NUM_ON_CPUS;
		for (i = 0; i < in_num; i++)
			cpu_plugin_onecore();
	} else if (NUM_ON_CPUS > real_max) {
		out_num = NUM_ON_CPUS - real_max;
		for (i = 0; i < out_num; i++)
			cpu_plugout_onecore();
	}
	cpunum_qos_unlock();
	return NOTIFY_OK;
}

static struct notifier_block cpunum_notifier = {
	.notifier_call = cpunum_notify,
};

static ssize_t min_core_num_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", pm_qos_request(PM_QOS_CPU_NUM_MIN));
}

static ssize_t min_core_num_set(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	s32 req_num;
	if (1 != sscanf(buf, "%d", &req_num))
		return -EINVAL;
	if (req_num < 1 || req_num > NUM_CPUS)
		return -EINVAL;
	pm_qos_update_request(&cpunum_qos_min, req_num);
	return count;
}

static DEVICE_ATTR(min_core_num, S_IRUGO | S_IWUSR, min_core_num_get,
		   min_core_num_set);

static ssize_t max_core_num_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", pm_qos_request(PM_QOS_CPU_NUM_MAX));
}

static ssize_t max_core_num_set(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	s32 req_num;
	if (1 != sscanf(buf, "%d", &req_num))
		return -EINVAL;
	if (req_num < 1 || req_num > NUM_CPUS)
		return -EINVAL;
	pm_qos_update_request(&cpunum_qos_max, req_num);
	return count;
}

static DEVICE_ATTR(max_core_num, S_IRUGO | S_IWUSR, max_core_num_get,
		   max_core_num_set);

static ssize_t available_core_num_get(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	s32 count = 0;
	int i;
	for (i = 1; i <= NUM_CPUS; i++)
		count += sprintf(&buf[count], "%d ", i);
	count += sprintf(&buf[count], "\n");
	return count;
}

static DEVICE_ATTR(available_core_num, S_IRUGO, available_core_num_get, NULL);

static ssize_t cur_core_num_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", NUM_ON_CPUS);
}

static DEVICE_ATTR(cur_core_num, S_IRUGO, cur_core_num_get, NULL);

static struct attribute *cpunum_attrs[] = {
	&dev_attr_min_core_num.attr,
	&dev_attr_max_core_num.attr,
	&dev_attr_available_core_num.attr,
	&dev_attr_cur_core_num.attr,
	NULL,
};

static struct attribute_group cpunum_attr_grp = {
	.attrs = cpunum_attrs,
	.name = "cpunum_qos",
};

static int __init cpunum_qos_init(void)
{
	int ret = 0;
	pr_info("%s, cpu numnber qos init function\n", __func__);

	ret = pm_qos_add_notifier(PM_QOS_CPU_NUM_MIN, &cpunum_notifier);
	if (ret) {
		pr_err("register min cpunum failed\n");
		return ret;
	}
	ret = pm_qos_add_notifier(PM_QOS_CPU_NUM_MAX, &cpunum_notifier);
	if (ret) {
		pr_err("register max cpunum failed\n");
		goto fail_max_notifier;
	}

	pm_qos_add_request(&cpunum_qos_min, PM_QOS_CPU_NUM_MIN, 1);
	pm_qos_add_request(&cpunum_qos_max, PM_QOS_CPU_NUM_MAX, NUM_CPUS);

	ret = sysfs_create_group(&(cpu_subsys.dev_root->kobj),
				 &cpunum_attr_grp);
	if (ret) {
		pr_err("Failed to register cpu num qos interface\n");
		goto fail_sysfs;
	}
	return 0;

fail_sysfs:
	pm_qos_remove_notifier(PM_QOS_CPU_NUM_MAX, &cpunum_notifier);
fail_max_notifier:
	pm_qos_remove_notifier(PM_QOS_CPU_NUM_MIN, &cpunum_notifier);
	return ret;
}

module_init(cpunum_qos_init);
