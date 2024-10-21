// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/sched/clock.h>
#include <linux/sched/mm.h>
#include <linux/sched/numa_balancing.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/trace_events.h>

#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "mt-plat/fpsgo_common.h"
#include "gbe1_usedext.h"
#include "fstb_usedext.h"
#include "gbe_common.h"
#include "gbe_sysfs.h"
#include "fpsgo_base.h"

enum GBE_NOTIFIER_PUSH_TYPE {
	GBE_NOTIFIER_SWITCH_GBE			= 0x00,
	GBE_NOTIFIER_RTID		= 0x01,
};

struct GBE_NOTIFIER_PUSH_TAG {
	enum GBE_NOTIFIER_PUSH_TYPE ePushType;

	struct hlist_head *list;
	int enable;

	struct work_struct sWork;
};

static DEFINE_MUTEX(gbe_list_lock);
static DEFINE_MUTEX(gbe_enable1_lock);
static struct workqueue_struct *g_psNotifyWorkQueue;
static int gbe_enable;

/* TODO: event register & dispatch */
int gbe_is_enable(void)
{
	int enable;

	mutex_lock(&gbe_enable1_lock);
	enable = gbe_enable;
	mutex_unlock(&gbe_enable1_lock);

	return enable;
}

void enable_gbe(int enable)
{
	pr_debug("[GBE] enable:%d %p\n", enable, &gbe_enable1_lock);
	mutex_lock(&gbe_enable1_lock);
	gbe_enable = !!enable;
	mutex_unlock(&gbe_enable1_lock);
}

void gbe_notify_fstb_poll(struct hlist_head *list)
{ }

#define MAX_GBE_BOOST_LIST_LENGTH 20
static int gbe_boost_list_length;
int set_gbe_boost_list(char *proc_name,
		char *thrd_name, unsigned long long runtime_thrs)
{
	struct GBE_BOOST_LIST *new_gbe_boost_list;
	int retval = 0;

	mutex_lock(&gbe_list_lock);

	if (!strncmp("0", proc_name, 1) &&
			!strncmp("0", thrd_name, 1) &&
			runtime_thrs == 0) {

		struct GBE_BOOST_LIST *iter;
		struct hlist_node *t;

		hlist_for_each_entry_safe(iter, t,
				&gbe_boost_list, hlist) {
			hlist_del(&iter->hlist);
			kfree(iter);
		}

		gbe_boost_list_length = 0;
		goto out;
	}

	if (gbe_boost_list_length >= MAX_GBE_BOOST_LIST_LENGTH) {
		retval = -ENOMEM;
		goto out;
	}

	new_gbe_boost_list =
		kzalloc(sizeof(*new_gbe_boost_list), GFP_KERNEL);
	if (new_gbe_boost_list == NULL) {
		retval = -ENOMEM;
		goto out;
	}

	if (!strncpy(
				new_gbe_boost_list->process_name,
				proc_name, 16)) {
		kfree(new_gbe_boost_list);
		retval = -ENOMEM;
		goto out;
	}
	new_gbe_boost_list->process_name[15] = '\0';

	if (!strncpy(
				new_gbe_boost_list->thread_name,
				thrd_name, 16)) {
		kfree(new_gbe_boost_list);
		retval = -ENOMEM;
		goto out;
	}
	new_gbe_boost_list->thread_name[15] = '\0';

	new_gbe_boost_list->runtime_thrs = runtime_thrs;

	hlist_add_head(&new_gbe_boost_list->hlist,
			&gbe_boost_list);

	gbe_boost_list_length++;

out:
	mutex_unlock(&gbe_list_lock);

	return retval;
}

static ssize_t gbe_enable1_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", gbe_is_enable());
}

static ssize_t gbe_enable1_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int arg;
	int val = 0;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	enable_gbe(val);

	return count;
}

static KOBJ_ATTR_RW(gbe_enable1);

static ssize_t gbe_boost_list1_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	struct GBE_BOOST_LIST *gbe_list_iter = NULL;
	char temp[GBE_SYSFS_MAX_BUFF_SIZE] = "";
	int pos = 0;
	int length;

	length = scnprintf(temp + pos, GBE_SYSFS_MAX_BUFF_SIZE - pos,
			"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
			"process_name",
			"thread_name",
			"pid",
			"tid",
			"runtime_thrs",
			"runtime_percent",
			"now_task_runtime",
			"boost_cnt");
	pos += length;

	hlist_for_each_entry(gbe_list_iter, &gbe_boost_list, hlist) {
		length = scnprintf(temp + pos, GBE_SYSFS_MAX_BUFF_SIZE - pos,
				"%s\t%s\t%d\t%d\t%llu\t\t%llu\t\t%llu\t\t%llu\n",
				gbe_list_iter->process_name,
				gbe_list_iter->thread_name,
				gbe_list_iter->pid,
				gbe_list_iter->tid,
				gbe_list_iter->runtime_thrs,
				gbe_list_iter->runtime_percent,
				gbe_list_iter->now_task_runtime,
				gbe_list_iter->boost_cnt);
		pos += length;
	}

	return scnprintf(buf, PAGE_SIZE, "%s", temp);
}

static ssize_t gbe_boost_list1_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int ret = count;
	char proc_name[16], thrd_name[16];
	unsigned long long runtime_thrs;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			acBuffer[count] = '\0';
			if (sscanf(acBuffer, "%15s %15s %llu",
						proc_name,
						thrd_name,
						&runtime_thrs) != 3) {
				goto err;
			}

			if (set_gbe_boost_list(proc_name,
					thrd_name, runtime_thrs))
				goto err;
		}
	}

err:
	return ret;
}

static KOBJ_ATTR_RW(gbe_boost_list1);

void gbe1_exit(void)
{
	gbe_sysfs_remove_file(&kobj_attr_gbe_enable1);
	gbe_sysfs_remove_file(&kobj_attr_gbe_boost_list1);
}

int gbe1_init(void)
{
	gbe_fstb2gbe_poll_fp = gbe_notify_fstb_poll;
	g_psNotifyWorkQueue =
		create_singlethread_workqueue("gbe_notifier_wq");

	gbe_sysfs_create_file(&kobj_attr_gbe_enable1);
	gbe_sysfs_create_file(&kobj_attr_gbe_boost_list1);

	return 0;
}

