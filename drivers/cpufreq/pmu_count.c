/* linux/drivers/cpufreq/pmu_count.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Samsung CPU Support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/cpumask.h>

#include "pmu_func.h"

static int start_counter(void *data);
static int stop_counter(void *data);

static DEFINE_MUTEX(pmu_counter_lock);
static int enable_pmu_mon = 1;
static int run;
static int events[7] = {0x77, 0x08, 0x17, };
static int alive_cpu[8];
static struct kobject *pmu_kobj;

static ssize_t event_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%x %x %x %x %x %x %x\n",
			events[0],
			events[1],
			events[2],
			events[3],
			events[4],
			events[5],
			events[6]);
}

static ssize_t event_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int i;

	pr_info("%s\n", buf);
	sscanf(buf, "%x %x %x %x %x %x %x",
			&events[0],
			&events[1],
			&events[2],
			&events[3],
			&events[4],
			&events[5],
			&events[6]);

	for (i=0; i<7; i++)
		pr_info("%d:[%x] ", i, events[i]);

	return count;
}

static struct kobj_attribute event_attribute = __ATTR(event, 0640, event_show, event_store);

static ssize_t run_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d: %s\n", run, run ? "run" : "no run");
}

static ssize_t run_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int cpu;
	struct task_struct *task;

	sscanf(buf, "%d", &run);

	if (run == 1) {
		for_each_online_cpu(cpu) {
			task = kthread_create(start_counter, NULL, "t_%u", cpu);
			kthread_bind(task, cpu);
			wake_up_process(task);
		}
	} else {
		for_each_online_cpu(cpu) {
			task = kthread_create(stop_counter, NULL, "t_%u", cpu);
			kthread_bind(task, cpu);
			wake_up_process(task);
		}
	}

	return count;
}

static struct kobj_attribute run_attribute = __ATTR(run, 0640, run_show, run_store);

static ssize_t enable_pmu_monitor_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d: %s\n", enable_pmu_mon,
				enable_pmu_mon ? "enabled" : "disabled");
}

static ssize_t enable_pmu_monitor_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	sscanf(buf, "%d", &enable_pmu_mon);

	return count;
}

static struct kobj_attribute enable_pmu_monitor_attribute =
		__ATTR(enable_pmu_monitor, 0640, enable_pmu_monitor_show, enable_pmu_monitor_store);

static ssize_t pmu_count_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "ccnt %llu, instcnt %u, l2refill %u\n",
				read_ccnt(), pmnc_read2(1), pmnc_read2(2));
}

static struct kobj_attribute pmu_count_attribute = __ATTR(pmu_count, 0440, pmu_count_show, NULL);

/* create group */
static struct attribute *attrs[] = {
	&run_attribute.attr,
	&event_attribute.attr,
	&enable_pmu_monitor_attribute.attr,
	&pmu_count_attribute.attr,
	NULL,
};

/* if a name, subdir will be created */
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static int start_counter(void *data)
{
	int i;

	if (!enable_pmu_mon)
		return 0;

	__start_timer();

	for (i = 0; i < 7; i++) {
		pmu_enable_counter(i);
		pmnc_config(i, events[i]);
		pr_info("%s: %d = %u\n", __func__, i, events[i]);
	}

	pmnc_reset();
	pr_info("pmu enabled\n");

	return 0;
}

static int stop_counter(void *data)
{
	int i;
	u32 v[7] = {0, };
	u64 ccnt = read_ccnt();

	if (!enable_pmu_mon)
		return 0;

	for (i = 0; i < 7; i++) {
		v[i] = pmnc_read2(i);
		pr_info("[%d] = %u, ccnt = %lu\n", i, v[i], (unsigned long)ccnt);
	}

	return 0;
}

int is_alive_cpu(int cpu)
{
	return alive_cpu[cpu];
}

int start_counter_cpu(int cpu)
{
	int i;

	if (!enable_pmu_mon)
		return 0;

	__start_timer();

	for (i = 0; i < 7; i++) {
		pmu_enable_counter(i);
		pmnc_config(i, events[i]);
	}

	alive_cpu[cpu] = 1;

	return 0;
}

int stop_counter_cpu(int cpu)
{
	if (!enable_pmu_mon)
		return 0;

	alive_cpu[cpu] = 0;

	return 0;
}

int read_pmu_one(void *data)
{
	struct pmu_count_value *p = (struct pmu_count_value *)data;

	if (!enable_pmu_mon)
		return 0;

	p->ccnt = read_ccnt();
	p->pmnc0 = pmnc_read2(0);
	p->pmnc1 = pmnc_read2(1);
	p->pmnc2 = pmnc_read2(2);
	p->pmnc3 = pmnc_read2(3);
	p->pmnc4 = pmnc_read2(4);
	p->pmnc5 = pmnc_read2(5);
	p->pmnc6 = pmnc_read2(6);
	p->valid = 1;

	reset_ccnt();
	pmnc_reset();

	return 0;
}

void read_counter_value(struct pmu_count_value *data)
{
	int i;
	struct task_struct *task;

	mutex_lock(&pmu_counter_lock);

	for (i = 0; i < 8 ; i++) {
		if (alive_cpu[i] == 1) {
			task = kthread_create(read_pmu_one, &data[i], "t_%u", i);
			kthread_bind(task, i);
			wake_up_process(task);
		} else {
			data[i].valid = 0;
		}
	}

	mutex_unlock(&pmu_counter_lock);
}

static int __init pmu_init(void)
{
	int ret;

	/* location: /sys/kernel/pmu_count */
	pmu_kobj = kobject_create_and_add("pmu_count", kernel_kobj);
	if (!pmu_kobj) {
		pr_info("%s: failed kobject_create_and_add\n", __func__);
		return -ENOMEM;
	}

	ret = sysfs_create_group(pmu_kobj, &attr_group);
	if (ret) {
		pr_info("%s: failed create sysfs group\n", __func__);
		kobject_put(pmu_kobj);
	}

	return ret;
}

static void __exit pmu_exit(void)
{
	sysfs_remove_group(pmu_kobj, &attr_group);
	kobject_put(pmu_kobj);
}

module_init(pmu_init);
module_exit(pmu_exit);
