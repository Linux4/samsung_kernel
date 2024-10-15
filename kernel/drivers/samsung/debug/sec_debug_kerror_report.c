// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_kerror_report.c
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
#include <trace/events/error_report.h>

#include "sec_debug_internal.h"

#define MAX_KERROR_TYPE	3

#define MAX_RECORD_NUM	8

#define CALLSTACK_ADDRS_COUNT 64

struct kerror_item {
	int id;
	int type;
	u64 time;
	void *address;
	unsigned long callstack[CALLSTACK_ADDRS_COUNT];
	unsigned int nr_stack;
	int cpu;
	int pid;
};

static struct kerror_item kerror_records[MAX_RECORD_NUM];

static atomic_t kerr_count;

static const char *get_kerror_name(int type)
{
	static const char * const kerror_type_names[] = {
		"KFENCE",
		"KASAN",
		"WARN",
	};

	BUILD_BUG_ON(ARRAY_SIZE(kerror_type_names) != MAX_KERROR_TYPE);

	if (type < 0 || type >= ARRAY_SIZE(kerror_type_names))
		return "Other";

	return kerror_type_names[type];
}

static void secdbg_kerror_rec_one(struct kerror_item *item,
				int id, int type, void *address)
{
	int cpu = raw_smp_processor_id();
	unsigned int nr_entries;

	item->id = id;
	item->type = type;
	item->time = cpu_clock(cpu);
	item->address = address;
	item->cpu = cpu;
	item->pid = task_pid_nr(current);

	nr_entries = stack_trace_save(item->callstack,
				      CALLSTACK_ADDRS_COUNT, 2);
	if (nr_entries < CALLSTACK_ADDRS_COUNT)
		item->callstack[nr_entries] = 0;
	item->nr_stack = nr_entries;
}

static void secdbg_kerror_record_error(int type, void *address)
{
	int id;
	unsigned long index;

	id = atomic_fetch_inc(&kerr_count);
	index = id & (MAX_RECORD_NUM - 1);

	pr_warn("secdbg: recorded: #%d %s %pS\n",
			id, get_kerror_name(type), address);

	secdbg_kerror_rec_one(&kerror_records[index], id, type, address);
}

static void secdbg_kerror_error_report_end(void *data,
		enum error_detector error_detector, unsigned long id)
{
	secdbg_kerror_record_error(error_detector, (void *)id);
}

static void secdbg_kerror_print_one_record(int i, struct kerror_item *item)
{
	u64 time = item->time;
	unsigned long rem_nsec = do_div(time, 1000000000);

	pr_info("%s #%d [%5lu.%06lu c%d:t%-5d]: at %pS\n",
		       get_kerror_name(item->type), item->id,
		       (unsigned long)time, rem_nsec / 1000, item->cpu, item->pid,
		       item->address);
	stack_trace_print(item->callstack, item->nr_stack, 0);
}

static int _idx_per_type[MAX_KERROR_TYPE + 1];

static void __init_idx_per_type(void)
{
	memset(_idx_per_type, 0x0, sizeof(_idx_per_type));
}

static int __get_idx_per_type(struct kerror_item *item)
{
	int type = item->type;

	if (type < 0 || type >= MAX_KERROR_TYPE)
		type = MAX_KERROR_TYPE;

	return _idx_per_type[type]++;
}

static int secdbg_kerror_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	int recorded = atomic_read(&kerr_count);
	int i, idx;

	pr_emerg("sec_debug: %s\n", __func__);

	if (!recorded)
		return NOTIFY_DONE;

	if (recorded > MAX_RECORD_NUM) {
		pr_err("kerr_count overflow: %d\n", recorded);
		recorded = MAX_RECORD_NUM;
	}

	__init_idx_per_type();

	pr_info("Recorded stacks\n");
	for (i = 0; i < recorded; i++) {
		idx = __get_idx_per_type(&kerror_records[i]);
		secdbg_kerror_print_one_record(idx, &kerror_records[i]);
	}

	return NOTIFY_DONE;
}

static struct notifier_block nb_panic_block = {
	.notifier_call = secdbg_kerror_panic_handler,
};

static __init int secdbg_kerror_init(void)
{
	int ret;

	register_trace_error_report_end(secdbg_kerror_error_report_end, NULL);

	ret = atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	if (ret)
		pr_err("%s: fail to register panic handler (%d)\n", __func__, ret);

	return 0;
}
late_initcall(secdbg_kerror_init);

MODULE_DESCRIPTION("Samsung Debug Checking Preempt driver");
MODULE_LICENSE("GPL v2");
