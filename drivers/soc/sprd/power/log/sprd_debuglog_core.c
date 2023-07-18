/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
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
#include <linux/cpu_pm.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/suspend.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "sprd_debuglog_core.h"

#define DEBUGLOG_MONITOR_INTERVAL	30

static struct task_struct *debug_task;
static struct debug_log *debug_log;

/**
 * sleep_notifier - Sleep condition check
 */
static int sleep_notifier(struct notifier_block *s, unsigned long cmd, void *v)
{
	struct debug_event *sleep_check;
	int ret;

	if (!debug_log) {
		pr_err("%s: The debug log is not register\n", __func__);
		return NOTIFY_STOP;
	}

	if (!debug_log->sleep_condition_check->handle) {
		dev_err(debug_log->dev, "The check event is null\n");
		return NOTIFY_BAD;
	}

	sleep_check = debug_log->sleep_condition_check;

	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		ret = sleep_check->handle(debug_log->dev, &sleep_check->data);
		if (ret)
			dev_warn(debug_log->dev, "Condition check error\n");
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

/**
 * sleep_notifier_block - Sleep condition check notifier
 */
static struct notifier_block sleep_notifier_block = {
	.notifier_call = sleep_notifier,
};

/**
 * source_notifier - Wakeup source get notifier
 */
static int source_notifier(struct notifier_block *s, unsigned long cmd, void *v)
{
	struct debug_event *source_get;
	int ret;

	if (!debug_log) {
		pr_err("%s: The debug log is not register\n", __func__);
		return NOTIFY_STOP;
	}

	if (!debug_log->wakeup_soruce_get->handle) {
		dev_err(debug_log->dev, "The get event is null\n");
		return NOTIFY_BAD;
	}

	source_get = debug_log->wakeup_soruce_get;

	switch (cmd) {
	case CPU_CLUSTER_PM_EXIT:
		ret = source_get->handle(debug_log->dev, &source_get->data);
		if (ret)
			dev_warn(debug_log->dev, "Wakeup source get error\n");
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

/**
 * debug_notifier_block - Power debug notifier block
 */
static struct notifier_block source_notifier_block = {
	.notifier_call = source_notifier,
};

/**
 * debug_notifier - Notification call back function
 */
static int print_notifier(struct notifier_block *s, unsigned long cmd, void *v)
{
	struct debug_event *source_print;
	int ret;

	if (!debug_log) {
		pr_err("%s: The debug log is not register\n", __func__);
		return NOTIFY_STOP;
	}

	if (!debug_log->wakeup_soruce_print->handle) {
		dev_err(debug_log->dev, "The print event is null\n");
		return NOTIFY_BAD;
	}

	source_print = debug_log->wakeup_soruce_print;

	switch (cmd) {
	case PM_POST_SUSPEND:
		ret = source_print->handle(debug_log->dev, &source_print->data);
		if (ret)
			dev_warn(debug_log->dev, "Wakeup print error\n");
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

/**
 * debug_notifier_block - Power debug notifier block
 */
static struct notifier_block print_notifier_block = {
	.notifier_call = print_notifier,
};

/**
 * debug_monitor_scan - the log output thread function
 */
static int state_monitor(void *data)
{
	struct debug_event *monitor;
	int ret;

	if (!debug_log) {
		pr_err("%s: The debug log is not register\n", __func__);
		return NOTIFY_STOP;
	}

	if (!debug_log->subsys_state_monitor->handle) {
		dev_err(debug_log->dev, "The monitor event is null\n");
		return NOTIFY_BAD;
	}

	monitor = debug_log->subsys_state_monitor;

	for (;;) {
		if (kthread_should_stop()) {
			dev_info(debug_log->dev, "The task is stop\n");
			break;
		}

		ret = monitor->handle(debug_log->dev, &monitor->data);
		if (ret)
			dev_warn(debug_log->dev, "State monitor error\n");

		schedule_timeout(DEBUGLOG_MONITOR_INTERVAL * HZ);
		set_current_state(TASK_INTERRUPTIBLE);
	}

	return 0;
}

/**
 * sprd_debug_log_register - debug log register interface
 */
int sprd_debug_log_register(struct debug_log *dbg)
{
	struct device *dev;
	int ret;

	pr_info("Register debug log\n");

	if (debug_log || !dbg || !dbg->dev) {
		pr_err("The debug log is already registered or the parameters are incorrect\n");
		return -EINVAL;
	}

	debug_log = dbg;
	dev = debug_log->dev;

	if (debug_log->sleep_condition_check) {
		ret = cpu_pm_register_notifier(&sleep_notifier_block);
		if (ret)
			dev_err(dev, "Sleep notifier register error\n");
	}

	if (debug_log->wakeup_soruce_get) {
		ret = cpu_pm_register_notifier(&source_notifier_block);
		if (ret)
			dev_err(dev, "Source notifier register error\n");
	}

	if (debug_log->wakeup_soruce_print) {
		ret = register_pm_notifier(&print_notifier_block);
		if (ret)
			dev_err(dev, "Print notifier register error\n");
	}

	if (debug_log->subsys_state_monitor) {
		debug_task = kthread_create(state_monitor, NULL, "state-check");
		if (IS_ERR(debug_task))
			dev_err(dev, "Create monitor task error\n");
		else
			wake_up_process(debug_task);
	}

	return 0;
}

/**
 * sprd_debug_log_unregister - debug log unregister interface
 */
int sprd_debug_log_unregister(void)
{
	if (!debug_log) {
		pr_err("The debug log is not registered\n");
		return -EINVAL;
	}

	if (debug_log->sleep_condition_check)
		cpu_pm_unregister_notifier(&sleep_notifier_block);

	if (debug_log->wakeup_soruce_get)
		cpu_pm_unregister_notifier(&source_notifier_block);

	if (debug_log->wakeup_soruce_print)
		unregister_pm_notifier(&print_notifier_block);

	if (debug_log->subsys_state_monitor)
		kthread_stop(debug_task);

	debug_task = NULL;
	debug_log  = NULL;

	return 0;
}
