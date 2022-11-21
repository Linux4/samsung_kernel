// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/stacktrace.h>
#include <linux/sched/debug.h>
#include <linux/sec_debug.h>

#include "../../../kernel/sched/sched.h"

#define MAX_CALL_ENTRY		128

static struct task_struct *suspect;

static inline char get_state(struct task_struct *p)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'X', 'Z', 'P', 'x', 'K', 'W', 'I', 'N'};
	unsigned char idx = 0;
	unsigned long state;

	if (!p)
		return 0;

	state = (p->state | p->exit_state) & (TASK_STATE_MAX - 1);

	while (state) {
		idx++;
		state >>= 1;
	}

	return state_array[idx];
}

#ifdef MODULE
DEFINE_STATIC_PR_AUTO_NAME_ONCE(callstack, ASL2);

static void __print_stack_trace(struct stack_trace *trace)
{
	unsigned int i;
	const int spaces = 0;

	if (!trace->nr_entries)
		return;

	pr_auto_name_once(callstack);
	pr_auto_name(callstack, "Call trace:\n");

	for (i = 0; i < trace->nr_entries; i++)
		pr_auto_name(callstack, "%*c%pS\n", 1 + spaces, ' ', (void *)trace->entries[i]);
}

static void __show_callstack(struct task_struct *tsk)
{
	unsigned long entry[MAX_CALL_ENTRY];
	struct stack_trace trace = {0,};

	trace.max_entries = MAX_CALL_ENTRY;
	trace.entries = entry;

	if (!tsk) {
		save_stack_trace(&trace);
	} else {
		trace.skip = 1;	/* skipping __switch_to */
		save_stack_trace_tsk(tsk, &trace);
	}

	if (!trace.nr_entries) {
		if (!tsk)
			pr_err("no trace for current\n");
		else
			pr_err("no trace for [%s :%d]\n", tsk->comm, tsk->pid);
		return;
	}

	__print_stack_trace(&trace);
}
#else /* MODULE */
static void __show_callstack(struct task_struct *task)
{
#ifdef CONFIG_SEC_DEBUG_AUTO_COMMENT
	show_stack_auto_comment(task, NULL);
#else
	show_stack(task, NULL);
#endif
}
#endif /* MODULE */

static void show_callstack(void *info)
{
	__show_callstack(NULL);
}

static struct task_struct *secdbg_softdog_find_key_suspect()
{
	struct task_struct *c, *g;

	for_each_process(g) {
		if (g->pid == 1)
			suspect = g;
		else if (!strcmp(g->comm, "system_server")) {
			suspect = g;
			for_each_thread(g, c) {
				if (!strcmp(c->comm, "watchdog")) {
					suspect = c;
					goto out;
				}
			}
			goto out;
		}
	}

out:
	return suspect;
}

void secdbg_softdog_show_info(void)
{
	struct task_struct *p = NULL;
	static call_single_data_t csd;

	p = secdbg_softdog_find_key_suspect();

	if (p) {
		pr_auto(ASL5, "[SOFTDOG] %s:%d %c(%d) exec:%lluns\n",
			p->comm, p->pid, get_state(p), (int)p->state, p->se.exec_start);

		if (task_running(task_rq(p), p)) {
			csd.flags = 0;
			csd.func = show_callstack;
			csd.info = NULL;
			smp_call_function_single_async(task_cpu(p), &csd);
		} else {
			__show_callstack(p);
		}
	} else {
		pr_auto(ASL5, "[SOFTDOG] Init task not exist!\n");
	}
}
EXPORT_SYMBOL_GPL(secdbg_softdog_show_info);

MODULE_DESCRIPTION("Samsung Debug softdog info driver");
MODULE_LICENSE("GPL v2");
