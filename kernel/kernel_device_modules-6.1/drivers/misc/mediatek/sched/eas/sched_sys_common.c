// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/seq_file.h>
#include <linux/energy_model.h>
#include <linux/topology.h>
#include <linux/sched/topology.h>
#include <trace/events/sched.h>
#include <trace/hooks/sched.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include "sched_sys_common.h"
#include <sugov/cpufreq.h>

#define CREATE_TRACE_POINTS

static struct attribute *sched_ctl_attrs[] = {
#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
	&sched_core_pause_info_attr.attr,
	&sched_turn_point_freq_attr.attr,
	&sched_target_margin_attr.attr,
	&sched_target_margin_low_attr.attr,
	&sched_util_est_ctrl.attr,
	&sched_am_ctrl.attr,
#endif
	&sched_gas_for_ta.attr,
	NULL,
};

static struct attribute_group sched_ctl_attr_group = {
	.attrs = sched_ctl_attrs,
};

static struct kobject *kobj;
int init_sched_common_sysfs(void)
{
	int ret = 0;

	kobj = kobject_create_and_add("sched_ctl",
				&cpu_subsys.dev_root->kobj);
	if (!kobj) {
		pr_info("sched_ctl folder create failed\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(kobj, &sched_ctl_attr_group);
	if (ret)
		goto error;
	kobject_uevent(kobj, KOBJ_ADD);

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
	task_rotate_init();
#endif

	return 0;

error:
	kobject_put(kobj);
	kobj = NULL;
	return ret;
}

void cleanup_sched_common_sysfs(void)
{
	if (kobj) {
		sysfs_remove_group(kobj, &sched_ctl_attr_group);
		kobject_put(kobj);
		kobj = NULL;
	}
}

static ssize_t show_sched_target_margin(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	int pd_count = 3, i;

	for (i = 0; i < pd_count; i++)
		len += snprintf(buf+len, max_len-len,
			"C%d=%d low:C%d=%d ", i, get_target_margin(i), i, get_target_margin_low(i));
	len += snprintf(buf + len, max_len-len, "\n");
	return len;
}

static ssize_t show_sched_turn_point_freq(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;
	int pd_count = 3, i;

	for (i = 0; i < pd_count; i++)
		len += snprintf(buf+len, max_len-len,
			"C%d=%lu ", i, get_turn_point_freq(i));
	len += snprintf(buf + len, max_len-len, "\n");
	return len;
}

ssize_t store_sched_turn_point_freq(struct kobject *kobj, struct kobj_attribute *attr,
const char __user *buf, size_t cnt)
{
	int cluster;
	int freq;

	if (sscanf(buf, "%d %d", &cluster, &freq) != 2)
		return -EINVAL;
	set_turn_point_freq(cluster, freq);
	return cnt;
}

ssize_t store_sched_target_margin(struct kobject *kobj, struct kobj_attribute *attr,
const char __user *buf, size_t cnt)
{
	int cluster;
	int value;

	if (sscanf(buf, "%d %d", &cluster, &value) != 2)
		return -EINVAL;
	set_target_margin(cluster, value);
	return cnt;
}

ssize_t store_sched_target_margin_low(struct kobject *kobj, struct kobj_attribute *attr,
const char __user *buf, size_t cnt)
{
	int cluster;
	int value;

	if (sscanf(buf, "%d %d", &cluster, &value) != 2)
		return -EINVAL;
	set_target_margin_low(cluster, value);
	return cnt;
}

extern bool sysctl_util_est;
ssize_t store_sched_util_est_ctrl(struct kobject *kobj, struct kobj_attribute *attr,
const char __user *buf, size_t cnt)
{
	int enable;

	if (kstrtouint(buf, 10, &enable))
		return -EINVAL;

	set_util_est_ctrl(enable);
	return cnt;
}

ssize_t show_sched_util_est_ctrl(struct kobject *kobj,
struct kobj_attribute *attr, char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len += snprintf(buf+len, max_len-len,
			"%d\n", sysctl_util_est);

	return len;
}

static ssize_t show_sched_am_ctrl(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len = snprintf(buf, max_len, "%d\n",  get_am_ctrl());
	if (len <= 0 || len >= max_len)
		return -EINVAL;

	return len;
}

ssize_t store_sched_am_ctrl(struct kobject *kobj, struct kobj_attribute *attr,
const char __user *buf, size_t cnt)
{
	int am_ctrl;

	if (kstrtouint(buf, 10, &am_ctrl))
		return -EINVAL;
	if (am_ctrl <0 || am_ctrl > 9)
		return -1;
	set_am_ctrl(am_ctrl);
	return cnt;
}

static ssize_t show_sched_gas_for_ta(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	unsigned int len = 0;
	unsigned int max_len = 4096;

	len = snprintf(buf, max_len, "%d\n",  get_top_grp_aware());
	return len;
}

ssize_t store_sched_gas_for_ta(struct kobject *kobj, struct kobj_attribute *attr,
const char __user *buf, size_t cnt)
{
	int enable;

	if (kstrtouint(buf, 10, &enable))
		return -EINVAL;
	if (enable < 0 || enable > 1)
		return -1;
	set_top_grp_aware(enable, 0);
	return cnt;
}

struct kobj_attribute sched_turn_point_freq_attr =
__ATTR(sched_turn_point_freq, 0640, show_sched_turn_point_freq, store_sched_turn_point_freq);
struct kobj_attribute sched_target_margin_attr =
__ATTR(sched_target_margin, 0640, show_sched_target_margin, store_sched_target_margin);
struct kobj_attribute sched_target_margin_low_attr =
__ATTR(sched_target_margin_low, 0640, show_sched_target_margin, store_sched_target_margin_low);
struct kobj_attribute sched_util_est_ctrl =
__ATTR(sched_util_est_ctrl, 0640, show_sched_util_est_ctrl, store_sched_util_est_ctrl);
struct kobj_attribute sched_am_ctrl =
__ATTR(am_ctrl, 0640, show_sched_am_ctrl, store_sched_am_ctrl);
struct kobj_attribute sched_gas_for_ta =
__ATTR(gas_for_ta, 0640, show_sched_gas_for_ta, store_sched_gas_for_ta);
