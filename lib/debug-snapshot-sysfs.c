/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos-SnapShot debugging framework for Exynos SoC
 *
 * Author: Hosung Kim <Hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/kallsyms.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include <linux/pstore_ram.h>
#include <linux/sched/clock.h>
#include <linux/ftrace.h>

#include "debug-snapshot-local.h"
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/hardirq.h>
#include <asm/stacktrace.h>
#include <linux/debug-snapshot.h>
#include <linux/kernel_stat.h>
#include <linux/irqnr.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>

/*
 *  sysfs implementation for debug-snapshot
 *  you can access the sysfs of debug-snapshot to /sys/devices/system/debug_snapshot
 *  path.
 */
static struct bus_type dss_subsys = {
	.name = "debug-snapshot",
	.dev_name = "debug-snapshot",
};

extern int dss_irqlog_exlist[DSS_EX_MAX_NUM];
extern int dss_irqexit_exlist[DSS_EX_MAX_NUM];
extern unsigned int dss_irqexit_threshold;

static ssize_t dss_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dbg_snapshot_item *item;
	unsigned long i;
	ssize_t n = 0;

	/*  item  */
	for (i = 0; i < dss_desc.log_cnt; i++) {
		item = &dss_items[i];
		n += scnprintf(buf + n, 24, "%-12s : %sable\n",
			item->name, item->entry.enabled ? "en" : "dis");
	}

	/*  base  */
	n += scnprintf(buf + n, 24, "%-12s : %sable\n",
			"base", dss_base.enabled ? "en" : "dis");

	return n;
}

static ssize_t dss_callstack_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n = scnprintf(buf, 24, "callstack depth : %d\n", dss_desc.callstack);

	return n;
}

static ssize_t dss_callstack_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long callstack;

	callstack = simple_strtoul(buf, NULL, 0);
	dev_info(dss_desc.dev, "callstack depth(min 1, max 4) : %lu\n", callstack);

	if (callstack < 5 && callstack > 0) {
		dss_desc.callstack = (unsigned int)callstack;
		dev_info(dss_desc.dev, "success inserting %lu to callstack value\n", callstack);
	}
	return count;
}

static ssize_t dss_irqlog_exlist_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned long i;
	ssize_t n = 0;

	n = scnprintf(buf, 24, "excluded irq number\n");

	for (i = 0; i < ARRAY_SIZE(dss_irqlog_exlist); i++) {
		if (dss_irqlog_exlist[i] == 0)
			break;
		n += scnprintf(buf + n, 24, "irq num: %-4d\n", dss_irqlog_exlist[i]);
	}
	return n;
}

static ssize_t dss_irqlog_exlist_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long i;
	unsigned long irq;

	irq = simple_strtoul(buf, NULL, 0);
	dev_info(dss_desc.dev, "irq number : %lu\n", irq);

	for (i = 0; i < ARRAY_SIZE(dss_irqlog_exlist); i++) {
		if (dss_irqlog_exlist[i] == 0)
			break;
	}

	if (i == ARRAY_SIZE(dss_irqlog_exlist)) {
		dev_err(dss_desc.dev, "list is full\n");
		return count;
	}

	if (irq != 0) {
		dss_irqlog_exlist[i] = irq;
		dev_info(dss_desc.dev, "success inserting %lu to list\n", irq);
	}
	return count;
}

#ifdef CONFIG_SEC_PM_DEBUG
static ssize_t dss_log_work_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return dss_log_work_print(buf);
}
#endif /* CONFIG_SEC_PM_DEBUG */

static struct device_attribute dss_enable_attr =
__ATTR(enabled, 0644, dss_enable_show, NULL);

static struct device_attribute dss_callstack_attr =
__ATTR(callstack, 0644, dss_callstack_show, dss_callstack_store);

static struct device_attribute dss_irqlog_attr =
__ATTR(exlist_irqdisabled, 0644, dss_irqlog_exlist_show,
					dss_irqlog_exlist_store);

#ifdef CONFIG_SEC_PM_DEBUG
static struct kobj_attribute dss_log_work_attr =
__ATTR(kworker, 0444, dss_log_work_show, NULL);
#endif /* CONFIG_SEC_PM_DEBUG */

static struct attribute *dss_sysfs_attrs[] = {
	&dss_enable_attr.attr,
	&dss_callstack_attr.attr,
	&dss_irqlog_attr.attr,
#ifdef CONFIG_SEC_PM_DEBUG
	&dss_log_work_attr.attr,
#endif /* CONFIG_SEC_PM_DEBUG */
	NULL,
};

static struct attribute_group dss_sysfs_group = {
	.attrs = dss_sysfs_attrs,
};

static const struct attribute_group *dss_sysfs_groups[] = {
	&dss_sysfs_group,
	NULL,
};

static int __init dbg_snapshot_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&dss_subsys, dss_sysfs_groups);
	if (ret)
		dev_err(dss_desc.dev, "fail to register debug-snapshop subsys\n");

	return ret;
}
late_initcall(dbg_snapshot_sysfs_init);
