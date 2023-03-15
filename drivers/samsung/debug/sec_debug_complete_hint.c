/*
 * sec_debug_complete_hint.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/sec_debug_complete_hint.h>
#include <linux/debug-snapshot.h>
#include <linux/sched/signal.h>
#include <linux/stacktrace.h>

#define LOG_LINE_MAX 256

enum {
	COMPLETE_IN_TASK,
	COMPLETE_IN_IRQ
};

void secdbg_hint_save_complete_hint(struct secdbg_hint *hint)
{
	struct stack_trace trace;
	hint->hint_idx = hint->hint_idx % MAX_HINT;

	trace.nr_entries = 0;
	trace.max_entries = CALLSTACK_MAX_NUM;
	trace.entries = (unsigned long *)hint->hint[hint->hint_idx].addrs;
	trace.skip = 0;
	save_stack_trace(&trace);

	hint->hint[hint->hint_idx].magic = HINT_MAGIC;
	if (hardirq_count()) {
		hint->hint[hint->hint_idx].fn = (void *)secdbg_snapshot_get_hardlatency_info(smp_processor_id());
		hint->hint[hint->hint_idx].type = COMPLETE_IN_IRQ;
	}
	else {
		hint->hint[hint->hint_idx].p = current;
		hint->hint[hint->hint_idx].type = COMPLETE_IN_TASK;
	}
	hint->hint_idx++;
}

void secdbg_hint_display_complete_hint()
{
	int i, j;
	char buf[LOG_LINE_MAX];
	struct task_struct *p;

	pr_info("\n");
	pr_info("Showing task having wait_for_completion\n");
	pr_info("------------------------------------------------------------------------------------------\n");
	pr_info("pid   comm        task_struct     completion         hint (task_struct/irq fn, call stack)\n");
	pr_info("------------------------------------------------------------------------------------------\n");

	rcu_read_lock();
	for_each_process(p) {
		if (!p->x)
			continue;

		pr_info("# %d %s %lx %lx\n", p->pid, p->comm, (unsigned long)p, (unsigned long)p->x);
	
		for(i = 0; i < MAX_HINT; i++) {
			ssize_t offset = 0;
			struct complete_hint * hint = &p->x->hint.hint[i];

			if (hint->magic != HINT_MAGIC)
				continue;
			if (!hint->p)
				continue;
			offset += snprintf(buf + offset, LOG_LINE_MAX, "HINT%d ", i+1);
			if (hint->type == COMPLETE_IN_TASK)
				offset += snprintf(buf + offset, LOG_LINE_MAX, "%d %s %px ", hint->p->pid, hint->p->comm, hint->p);
			else
				offset += snprintf(buf + offset, LOG_LINE_MAX, "%pF ", hint->fn);
			for (j = 0; j < CALLSTACK_MAX_NUM; j++) {
				offset += snprintf(buf + offset, LOG_LINE_MAX, "%c %pF ", (j==0) ? ' ' : '<', hint->addrs[j]);
			}
			pr_info("%s\n", buf);
		}
	}
	rcu_read_unlock();
	pr_info("------------------------------------------------------------------------------------------\n");
}
