// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_check_preempt.c
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/panic_notifier.h>
#include <linux/sched/clock.h>
#include <linux/sec_debug.h>

#include "sec_debug_internal.h"

#define MAX_RECORD_NUM	32

#define CALLSTACK_ADDRS_COUNT 16

struct preempt_item {
	int type;
	u64 time;
	void *handler;
	unsigned int old_count;
	unsigned int new_count;
	unsigned long callstack[CALLSTACK_ADDRS_COUNT];
	int cpu;
	int pid;
};

static struct preempt_item preempt_records[MAX_RECORD_NUM];

static atomic_t prec_count;
static bool prec_overflow;

static const char *get_pcheck_name(int type)
{
	static const char * const pcheck_type_names[] = {
		"None",
		"IRQ",
		"HANDLE_IRQ",
		"TIMER",
		"HRTIMER",
		"TASKLET",
		"SOFTIRQ",
		"EXIT_TO_USER",
	};

	BUILD_BUG_ON(ARRAY_SIZE(pcheck_type_names) != MAX_PCHECK_TYPE);

	if (type < 0 || type >= ARRAY_SIZE(pcheck_type_names))
		return "Invalid";

	return pcheck_type_names[type];
}

void secdbg_ckpc_preempt_mismatch(int type, unsigned int old_val, unsigned int new_val, void *func)
{
	int index;
	int cpu = raw_smp_processor_id();
	unsigned int nr_entries;

	pr_warn("secdbg: preempt mismatch: [%s] %pS (0x%x->0x%x)\n",
			get_pcheck_name(type), func, old_val, new_val);

	if (atomic_read(&prec_count) >= MAX_RECORD_NUM) {
		prec_overflow = true;
		return;
	}

	index = atomic_fetch_inc(&prec_count);
	if (index >= MAX_RECORD_NUM) {
		atomic_dec(&prec_count);
		prec_overflow = true;
		return;
	}

	preempt_records[index].type = type;
	preempt_records[index].time = cpu_clock(cpu);
	preempt_records[index].handler = func;
	preempt_records[index].old_count = old_val;
	preempt_records[index].new_count = new_val;
	preempt_records[index].cpu = cpu;
	preempt_records[index].pid = task_pid_nr(current);

	nr_entries = stack_trace_save(preempt_records[index].callstack,
				      CALLSTACK_ADDRS_COUNT, 1);
	if (nr_entries < CALLSTACK_ADDRS_COUNT)
		preempt_records[index].callstack[nr_entries] = 0;
}

static void secdbg_ckpc_print_one_record(int i, struct preempt_item *item)
{
	u64 time = item->time;
	unsigned long rem_nsec = do_div(time, 1000000000);

	pr_info(" %3d) %5lu.%06lu c%d:t%-5d [%s] %pS (0x%x->0x%x)\n",
		       i + 1, (unsigned long)time, rem_nsec / 1000,
		       item->cpu, item->pid,
		       get_pcheck_name(item->type), item->handler,
		       item->old_count, item->new_count);
}

static int secdbg_ckpc_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	int recorded = atomic_read(&prec_count);
	int i;

	pr_emerg("sec_debug: %s\n", __func__);

	if (!recorded)
		return NOTIFY_DONE;

	if (recorded > MAX_RECORD_NUM) {
		pr_err("prec_count overflow: %d\n", recorded);
		recorded = MAX_RECORD_NUM;
	}

	pr_info("Preempt mismatch %d items%s\n", recorded,
			prec_overflow ? " (more)" : "");
	for (i = 0; i < recorded; i++)
		secdbg_ckpc_print_one_record(i, &preempt_records[i]);

	return NOTIFY_DONE;
}

static struct notifier_block nb_panic_block = {
	.notifier_call = secdbg_ckpc_panic_handler,
};

static __init int secdbg_ckpc_init(void)
{
	int ret;

	ret = atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	if (ret)
		pr_err("%s: fail to register panic handler (%d)\n", __func__, ret);

	return 0;
}
late_initcall(secdbg_ckpc_init);

MODULE_DESCRIPTION("Samsung Debug Checking Preempt driver");
MODULE_LICENSE("GPL v2");
