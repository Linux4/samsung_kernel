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
#include <linux/sched/debug.h>
#include <linux/sec_debug.h>
#include <linux/sched/cputime.h>

#include "../../../kernel/sched/sched.h"
#include "sec_debug_internal.h"

static struct task_struct *suspect;

static char get_state(struct task_struct *p)
{
	static const char state_array[] = {'R', 'S', 'D', 'T', 't', 'X', 'Z', 'P', 'x', 'K', 'W', 'I', 'N', 'd', ' ', ' ', 'F'};
	unsigned char idx = 0;
	unsigned long state;

	BUILD_BUG_ON(1 + ilog2(TASK_STATE_MAX) != sizeof(state_array));

	if (!p)
		return 0;

	state = (READ_ONCE(p->__state) | p->exit_state) & (TASK_STATE_MAX - 1);
	state &= ~TASK_FREEZABLE_UNSAFE; /* ignore */

	while (state) {
		idx++;
		state >>= 1;
	}

	return state_array[idx];
}

#ifdef MODULE
static void __show_callstack(struct task_struct *task)
{
	secdbg_stra_show_callstack_auto(task);
}
#else /* MODULE */
static void __show_callstack(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
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

static struct task_struct *secdbg_softdog_find_key_suspect(void)
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
		unsigned long long last_arrival = p->sched_info.last_arrival;
		unsigned long long last_queued = p->sched_info.last_queued;

		unsigned long last_arrival_nsec = do_div(last_arrival, 1000000000);
		unsigned long last_queued_nsec = do_div(last_queued, 1000000000);


		pr_auto(ASL5, "[SOFTDOG] %s:%d %c(%u) last_arrival:%5lu.%06lus last_queued:%5lu.%06lus\n",
			p->comm, p->pid, get_state(p), READ_ONCE(p->__state),
			(unsigned long)last_arrival, last_arrival_nsec / 1000,
			(unsigned long)last_queued, last_queued_nsec / 1000);

		if (task_on_cpu(task_rq(p), p)) {
			INIT_CSD(&csd, show_callstack, NULL);
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
