// SPDX-License-Identifier: GPL-2.0
/*
 *  Copyright (C) 2022 MediaTek Inc.
 */

#include <linux/preempt.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/proc_fs.h>

#include "internal.h"

#define IRQ_LOG_START 0
#define IRQ_LOG_ENTRY 256
#define IRQ_LOG_END UINT_MAX

struct irq_log_entry {
	const char *func;
	int line;
	u64 ts;
};

struct irq_log_data {
	unsigned int count;
	struct irq_log_entry *entry;
};

static struct irq_log_data __percpu *irq_log_data;

void irq_log_start(void)
{
	this_cpu_write(irq_log_data->count, IRQ_LOG_START);
}

void irq_log_end(void)
{
	this_cpu_write(irq_log_data->count, IRQ_LOG_END);
}

void __irq_log_store(const char *func, int line)
{
	struct irq_log_data *data;
	int i;

	if (unlikely(!irqs_disabled()))
		return;

	data = this_cpu_ptr(irq_log_data);
	if (data->count >= IRQ_LOG_ENTRY)
		return;

	/* irqs is disabled and there should be no race condition below
	 * iff this function will not be called from NMI context
	 */
	i = data->count;
	data->count = i + 1;
	data->entry[i].func = func;
	data->entry[i].line = line;
	data->entry[i].ts = sched_clock();
}
EXPORT_SYMBOL_GPL(__irq_log_store);

void irq_log_dump(unsigned int out, u64 start, u64 end)
{
	struct irq_log_data *data = this_cpu_ptr(irq_log_data);
	u64 prev_ts = start;
	int i;
	int cpu = smp_processor_id();

	if (data->count > IRQ_LOG_ENTRY)
		return;

	/* It is safe to print the function pointers iff we are in the same
	 * interrupt context
	 */
	for (i = 0; i < data->count; i++) {
		irq_mon_msg(out,
			    "cpu=%d, func=%s, line=%d, time=%llu, delta=%llu",
			    cpu,
			    data->entry[i].func,
			    data->entry[i].line,
			    data->entry[i].ts,
			    data->entry[i].ts - prev_ts);
		prev_ts = data->entry[i].ts;
	}

	irq_mon_msg(out, "cpu=%d, end of IRQ, time=%llu, delta=%llu",
		    cpu, end, end - prev_ts);
}

void irq_log_exit(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		kfree(per_cpu_ptr(irq_log_data->entry, cpu));

	free_percpu(irq_log_data);
}

int irq_log_init(void)
{
	struct irq_log_data *data;
	int cpu;

	irq_log_data = alloc_percpu(typeof(*irq_log_data));
	if (!irq_log_data)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {
		data = per_cpu_ptr(irq_log_data, cpu);
		data->entry = kcalloc(IRQ_LOG_ENTRY, sizeof(*data->entry),
				      GFP_KERNEL);
		if (!data->entry) {
			irq_log_exit();
			return -ENOMEM;
		}
	}
	return 0;
}
