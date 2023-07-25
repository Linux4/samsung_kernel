// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/samsung/debug/sec_debug_sched_log.c
 *
 * COPYRIGHT(C) 2017-2019 Samsung Electronics Co., Ltd. All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/io.h>

#include <linux/sec_debug.h>

#include "sec_debug_internal.h"

DEFINE_PER_CPU(struct sec_debug_log, sec_debug_log_cpu);
struct sec_debug_last_pet_ns *secdbg_last_pet_ns __read_mostly; 

phys_addr_t secdbg_paddr;
size_t secdbg_size;

static int __init sec_dbg_setup(char *str)
{
	size_t size = (size_t)memparse(str, &str);

	pr_info("str=%s\n", str);

	if (size /*&& (size == roundup_pow_of_two(size))*/ && (*str == '@')) {
		secdbg_paddr = (phys_addr_t)memparse(++str, NULL);
		secdbg_size = size;
	}

	pr_info("secdbg_paddr = %pa\n", &secdbg_paddr);
	pr_info("secdbg_size = 0x%zx\n", secdbg_size);

	return 0;
}
__setup("sec_dbg=", sec_dbg_setup);

/* save last_pet and last_ns with these nice functions */
void sec_debug_save_last_pet(unsigned long long last_pet)
{
	if (likely(secdbg_last_pet_ns))
		secdbg_last_pet_ns->last_pet = last_pet;
}

void notrace sec_debug_save_last_ns(unsigned long long last_ns)
{
	if (likely(secdbg_last_pet_ns))
		atomic64_set(&(secdbg_last_pet_ns->last_ns), last_ns);
}

static inline long get_switch_state(bool preempt, struct task_struct *p)
{
	return preempt ? TASK_RUNNING | TASK_STATE_MAX : p->state;
}

void sec_debug_task_sched_log(int cpu, bool preempt,
		struct task_struct *task, struct task_struct *prev)
{
	struct sched_buf *sched_buf;
	int i;
	struct sec_debug_log *sec_dbg_log;
	sec_dbg_log = &per_cpu(sec_debug_log_cpu, cpu);

	if (unlikely(!sec_dbg_log))
		return;

	i = ++(sec_dbg_log->sched.idx) & (SCHED_LOG_MAX - 1);
	sched_buf = &sec_dbg_log->sched.buf[i];

	sched_buf->time = cpu_clock(cpu);
	sec_debug_strcpy_task_comm(sched_buf->comm, task->comm);
	sched_buf->pid = task->pid;
	sched_buf->pTask = task;
	sched_buf->prio = task->prio;
	sec_debug_strcpy_task_comm(sched_buf->prev_comm, prev->comm);

	sched_buf->prev_pid = prev->pid;
	sched_buf->prev_state = get_switch_state(preempt, prev);
	sched_buf->prev_prio = prev->prio;
}

void sec_debug_softirq_sched_log(unsigned int irq, void *fn,
		char *name, unsigned int en)
{
	struct irq_buf *irq_buf;
	int cpu = smp_processor_id();
	int i;
	struct sec_debug_log *sec_dbg_log;
	sec_dbg_log = &per_cpu(sec_debug_log_cpu, cpu);
	if (unlikely(!sec_dbg_log))
		return;

	i = ++(sec_dbg_log->irq.idx) & (SCHED_LOG_MAX - 1);
	irq_buf = &sec_dbg_log->irq.buf[i];

	irq_buf->time = cpu_clock(cpu);
	irq_buf->irq = irq;
	irq_buf->fn = fn;
	irq_buf->name = name;
	irq_buf->context = &cpu;
	irq_buf->en = irqs_disabled();
	irq_buf->preempt_count = preempt_count();
	irq_buf->pid = current->pid;
	irq_buf->entry_exit = en;
}

void sec_debug_irq_sched_log(unsigned int irq, void *desc_or_fn,
		void *action_or_name, unsigned int en)
{
	struct irq_buf *irq_buf;
	int cpu = smp_processor_id();
	int i;
	struct irq_desc *desc = (struct irq_desc *)desc_or_fn;
	struct irqaction *action = (struct irqaction *)action_or_name;

	struct sec_debug_log *sec_dbg_log;
	sec_dbg_log = &per_cpu(sec_debug_log_cpu, cpu);
	if (unlikely(!sec_dbg_log))
		return;

	i = ++(sec_dbg_log->irq.idx) & (SCHED_LOG_MAX - 1);
	irq_buf = &sec_dbg_log->irq.buf[i];

	irq_buf->time = cpu_clock(cpu);
	irq_buf->irq = irq;
	irq_buf->fn = action->handler;
	irq_buf->name = (char *)action->name;
	irq_buf->hwirq = desc->irq_data.hwirq;
	irq_buf->en = irqs_disabled();
	irq_buf->preempt_count = preempt_count();
	irq_buf->pid = current->pid;
	irq_buf->entry_exit = en;
}

void __deprecated sec_debug_irq_enterexit_log(unsigned int irq, u64 start_time){ return; }
void __deprecated sec_debug_timer_log(unsigned int type, int int_lock, void *fn){ return; }
void __deprecated sec_debug_secure_log(u32 svc_id, u32 cmd_id) { return; }

int sec_debug_sched_msg(char *fmt, ...)
{
	int cpu = raw_smp_processor_id();
	struct sched_buf *sched_buf;
	int r = 0;
	int i;
	va_list args;
	struct sec_debug_log *sec_dbg_log;
	sec_dbg_log = &per_cpu(sec_debug_log_cpu, cpu);
	if (unlikely(!sec_dbg_log))
		return 0;

	i = ++(sec_dbg_log->sched.idx) & (SCHED_LOG_MAX - 1);
	sched_buf = &sec_dbg_log->sched.buf[i];

	va_start(args, fmt);
	if (fmt) {
		r = vsnprintf(sched_buf->comm, sizeof(sched_buf->comm), fmt, args);
	} else {
		sched_buf->addr = va_arg(args, u64);
	}
	va_end(args);

	sched_buf->time = cpu_clock(cpu);
	sched_buf->pid = current->pid;
	sched_buf->pTask = NULL;

	return r;
}

#ifdef CONFIG_SEC_DEBUG_MSG_LOG
int ___sec_debug_msg_log(void *caller, const char *fmt, ...)
{
	struct secmsg_buf *secmsg_buf;
	int cpu = smp_processor_id();
	int r;
	int i;
	va_list args;
	struct sec_debug_log *sec_dbg_log;
	sec_dbg_log = &per_cpu(sec_debug_log_cpu, cpu);
	if (unlikely(!sec_dbg_log))
		return 0;

	i = ++(sec_dbg_log->secmsg.idx) & (MSG_LOG_MAX - 1);
	secmsg_buf = &sec_dbg_log->secmsg.buf[i];

	secmsg_buf->time = cpu_clock(cpu);
	va_start(args, fmt);
	r = vsnprintf(secmsg_buf->msg, sizeof(secmsg_buf->msg), fmt, args);
	va_end(args);

	secmsg_buf->caller0 = __builtin_return_address(0);
	secmsg_buf->caller1 = caller;
	secmsg_buf->task = current->comm;

	return r;
}
#endif /* CONFIG_SEC_DEBUG_MSG_LOG */

#ifdef CONFIG_SEC_DEBUG_AVC_LOG
int __deprecated sec_debug_avc_log(const char *fmt, ...) { return 0; }
#endif /* CONFIG_SEC_DEBUG_AVC_LOG */

#ifdef CONFIG_SEC_DEBUG_DCVS_LOG
void __deprecated sec_debug_dcvs_log(int cpu_no, unsigned int prev_freq,
		unsigned int new_freq)
{
	return;
}
#endif

#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
void __deprecated sec_debug_fuelgauge_log(unsigned int voltage,
		unsigned short soc, unsigned short charging_status)
{
	return;
}
#endif

#ifdef CONFIG_SEC_DEBUG_POWER_LOG
void sec_debug_cpu_lpm_log(int cpu, unsigned int index,
		bool success, int entry_exit)
{
	struct power_buf *power_buf;
	int i;
	struct sec_debug_log *sec_dbg_log;
	sec_dbg_log = &per_cpu(sec_debug_log_cpu, cpu);
	if (unlikely(!sec_dbg_log))
		return;

	i = ++(sec_dbg_log->pwr.idx) & (POWER_LOG_MAX - 1);
	power_buf = &sec_dbg_log->pwr.buf[i];
	power_buf->time = cpu_clock(cpu);
	power_buf->pid = current->pid;
	power_buf->type = CPU_POWER_TYPE;

	power_buf->cpu.index = index;
	power_buf->cpu.success = success;
	power_buf->cpu.entry_exit = entry_exit;
}

void sec_debug_cluster_lpm_log(const char *name, int index,
		unsigned long sync_cpus, unsigned long child_cpus,
		bool from_idle, int entry_exit)
{
	struct power_log *power_log;
	int i;
	int cpu = smp_processor_id();
	struct sec_debug_log *sec_dbg_log;
	sec_dbg_log = &per_cpu(sec_debug_log_cpu, cpu);
	if (unlikely(!sec_dbg_log))
		return;

	i = ++(sec_dbg_log->pwr.idx) & (POWER_LOG_MAX - 1);
	power_log = &sec_dbg_log->pwr.buf[i];

	power_buf->time = cpu_clock(cpu);
	power_buf->pid = current->pid;
	power_buf->type = CLUSTER_POWER_TYPE;

	power_buf->cluster.name = (char *) name;
	power_buf->cluster.index = index;
	power_buf->cluster.sync_cpus = sync_cpus;
	power_buf->cluster.child_cpus = child_cpus;
	power_buf->cluster.from_idle = from_idle;
	power_buf->cluster.entry_exit = entry_exit;
}

void sec_debug_clock_log(const char *name,
		unsigned int state, unsigned int cpu_id, int complete)
{
	struct power_log *power_log;
	int i;

	struct sec_debug_log *sec_dbg_log;
	sec_dbg_log = &per_cpu(sec_debug_log_cpu, cpu_id);
	if (unlikely(!sec_dbg_log))
		return;

	i = ++(sec_dbg_log->pwr.log) & (POWER_LOG_MAX - 1);
	power_log = &sec_dbg_log->pwr.buf[i];

	power_buf->time = cpu_clock(cpu_id);
	power_buf->pid = current->pid;
	power_buf->type = CLOCK_RATE_TYPE;

	power_buf->clk_rate.name = (char *)name;
	power_buf->clk_rate.state = state;
	power_buf->clk_rate.cpu_id = cpu_id;
	power_buf->clk_rate.complete = complete;
}
#endif /* CONFIG_SEC_DEBUG_POWER_LOG */

static int __init sec_debug_sched_log_init(void)
{
	size_t i;
	struct sec_debug_last_pet_ns *vaddr;
	struct sec_debug_log *sec_dbg_log;
	size_t size;

	if (secdbg_paddr == 0 || secdbg_size == 0) {
		pr_info("sec debug buffer not provided. Using kmalloc..\n");
		size = sizeof(struct sec_debug_last_pet_ns);
		vaddr = kzalloc(size, GFP_KERNEL);
	} else {
		size = secdbg_size;
		if (sec_debug_is_enabled())
			vaddr = ioremap_wc(secdbg_paddr, secdbg_size);
		else
			vaddr = ioremap_cache(secdbg_paddr, secdbg_size);
	}

	pr_info("vaddr=0x%p paddr=%pa size=0x%zx sizeof(struct sec_debug_last_pet_ns)=0x%zx\n",
			vaddr, &secdbg_paddr,
			secdbg_size, sizeof(struct sec_debug_last_pet_ns));

	if ((!vaddr) || (sizeof(struct sec_debug_last_pet_ns) > size)) {
		pr_err("ERROR! init failed!\n");
		return -EFAULT;
	}

	for (i = 0; i < num_possible_cpus(); i++) {
		sec_dbg_log = &per_cpu(sec_debug_log_cpu, i);
		sec_dbg_log->sched.idx = UINT_MAX;
		sec_dbg_log->irq.idx = UINT_MAX;

#ifdef CONFIG_SEC_DEBUG_MSG_LOG
		sec_dbg_log->secmsg.idx = UINT_MAX;
#endif

#ifdef CONFIG_SEC_DEBUG_POWER_LOG
		sec_dbg_log->pwr.idx = UINT_MAX;
#endif
	}

	secdbg_last_pet_ns = vaddr;

	pr_info("init done\n");

	return 0;
}
arch_initcall_sync(sec_debug_sched_log_init);
