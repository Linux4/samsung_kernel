// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 * author: Myong Jae Kim
 * email: myoungjae.kim@samsung.com
 */

#define pr_fmt(fmt) "[BSP] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sec_debug.h>
#include <linux/notifier.h>
#include <linux/sched/debug.h>
#include <linux/seq_file.h>
#include <linux/stacktrace.h>
#include "sec_debug_bspinfo.h"

#define CALLSTACK_SIZE		4096
#define MAX_CALL_ENTRY		128

static char callstack_buf[CONFIG_VENDOR_NR_CPUS][CALLSTACK_SIZE];

static void sshow_stack(char *s, size_t size, struct task_struct *tsk, unsigned long *sp, const char *loglvl)
{
	unsigned long entry[MAX_CALL_ENTRY];
	unsigned int nr_entries = 0;
	unsigned int i;
	int pos = 0;

	nr_entries = stack_trace_save(entry, ARRAY_SIZE(entry), 0);

	for (i = 0; i < nr_entries; i++)
		pos += snprintf(s + pos, size - pos, " %pSb\n", (void *)entry[i]);
}

static void sshowacpu(void *dummy)
{
	int cpu = smp_processor_id();
	int len = 0;

	/* Idle CPUs have no interesting backtrace. */
	if (idle_cpu(cpu)) {

		sprintf(callstack_buf[cpu], "CPU%d: backtrace skipped as idling\n", cpu);
		return;
	}

	len = snprintf(callstack_buf[cpu], CALLSTACK_SIZE - len,
		"CPU%d: %s (pid:%d, prio:%d)\n",
		cpu,
		current->comm,
		current->pid,
		current->prio);
	sshow_stack(callstack_buf[cpu] + len, CALLSTACK_SIZE - len, NULL, NULL, KERN_DEFAULT);
}

#if IS_ENABLED(CONFIG_SEC_DEBUG_BSPINFO)
static int callback_cur_tasks(struct seq_file *dst)
{
	int i;

	smp_call_function(sshowacpu, NULL, 1);

	for (i = 0; i < CONFIG_VENDOR_NR_CPUS; i++)
		seq_printf(dst, "%s\n", callstack_buf[i]);

	return NOTIFY_DONE;
}

BSP_INFO_DISPLAY_FN(cur_tasks, "Current tasks information", callback_cur_tasks);
#endif
static int __init secdbg_cur_tasks_init(void)
{
	int ret;

	pr_info("%s: init\n", __func__);

#if IS_ENABLED(CONFIG_SEC_DEBUG_BSPINFO)
	ret = secdbg_bspinfo_register_info_displayer(&cur_tasks_display_notifier);
	if (ret) {
		pr_info("%s: failed to register cur_tasks_display_notifier\n", __func__);
		return 0;
	}
#endif

	return 0;
}
late_initcall_sync(secdbg_cur_tasks_init);

static void __exit secdbg_cur_tasks_exit(void)
{

}
module_exit(secdbg_cur_tasks_exit);

MODULE_DESCRIPTION("Samsung Debug BSP Info driver");
MODULE_LICENSE("GPL v2");
#if IS_ENABLED(CONFIG_SEC_DEBUG_BSPINFO)
MODULE_SOFTDEP("pre: secdbg-bspinfo");
#endif

