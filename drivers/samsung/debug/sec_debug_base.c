/*
 * Copyright (c) 2014-2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <linux/of_reserved_mem.h>
#include <linux/memblock.h>
#include <linux/sched/task.h>
#include <linux/sec_debug.h>
#include <linux/sec_hard_reset_hook.h>
#include <asm/cacheflush.h>
#include <asm/stacktrace.h>
#include <linux/reboot.h>
#include <linux/sec_debug_complete_hint.h>
#include "sec_debug_internal.h"

/* layout of SDRAM : First 4KB of DRAM
 *         0x0: magic            (4B)
 */

enum sec_debug_upload_magic_t {
	UPLOAD_MAGIC_INIT		= 0x0,
	UPLOAD_MAGIC_PANIC		= 0x66262564,
};

/* TODO: masking ? */
enum sec_debug_upload_cause_t {
	UPLOAD_CAUSE_INIT		= 0xCAFEBABE,
	UPLOAD_CAUSE_KERNEL_PANIC	= 0x000000C8,
	UPLOAD_CAUSE_CP_ERROR_FATAL	= 0x000000CC,
	UPLOAD_CAUSE_USER_FAULT		= 0x0000002F,
	UPLOAD_CAUSE_HARD_RESET		= 0x00000066,
	UPLOAD_CAUSE_FORCED_UPLOAD	= 0x00000022,
	UPLOAD_CAUSE_USER_FORCED_UPLOAD	= 0x00000074,
};

static struct sec_debug_next *sdn;

static unsigned long secdbg_base_rmem_virt;
static unsigned long secdbg_base_rmem_phys;
static unsigned long secdbg_base_rmem_size;
static unsigned long secdbg_sdn_phys;
static unsigned long secdbg_sdn_size;

/* set magic */
static void secdbg_base_set_upload_magic(unsigned int magic, char *str)
{
	*(unsigned int *)secdbg_base_rmem_virt = magic;

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	if (str) {
		secdbg_exin_set_panic(str);
		secdbg_exin_set_finish();
	}
#endif

	pr_emerg("sec_debug: set magic code (0x%x)\n", magic);
}
/* get time function to log */
#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

#ifdef arch_idle_time
static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}
#else
static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	if (cpu_online(cpu))
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = idle_time * NSEC_PER_USEC;

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	if (cpu_online(cpu))
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = iowait_time * NSEC_PER_USEC;

	return iowait;
}
#endif

static void secdbg_base_dump_cpu_stat(void)
{
	int i, j;
	u64 user = 0;
	u64 nice = 0;
	u64 system = 0;
	u64 idle = 0;
	u64 iowait = 0;
	u64 irq = 0;
	u64 softirq = 0;
	u64 steal = 0;
	u64 guest = 0;
	u64 guest_nice = 0;
	u64 sum = 0;
	u64 sum_softirq = 0;
	unsigned int per_softirq_sums[NR_SOFTIRQS] = {0};
	const char *softirq_to_name[NR_SOFTIRQS] = {
		"HI", "TIMER", "NET_TX", "NET_RX", "BLOCK",
		"BLOCK_IOPOLL", "TASKLET", "SCHED", "HRTIMER", "RCU"
	};

	for_each_possible_cpu(i) {
		user	+= kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice	+= kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system	+= kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle	+= get_idle_time(i);
		iowait	+= get_iowait_time(i);
		irq	+= kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq	+= kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal	+= kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest	+= kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		sum	+= kstat_cpu_irqs_sum(i);
		sum	+= arch_irq_stat_cpu(i);

		for (j = 0; j < NR_SOFTIRQS; j++) {
			unsigned int softirq_stat = kstat_softirqs_cpu(j, i);

			per_softirq_sums[j] += softirq_stat;
			sum_softirq += softirq_stat;
		}
	}
	sum += arch_irq_stat();

	pr_info("\n");
	pr_info("cpu   user:%llu \tnice:%llu \tsystem:%llu \tidle:%llu \tiowait:%llu \tirq:%llu \tsoftirq:%llu \t %llu %llu %llu\n",
		(unsigned long long)nsec_to_clock_t(user),
		(unsigned long long)nsec_to_clock_t(nice),
		(unsigned long long)nsec_to_clock_t(system),
		(unsigned long long)nsec_to_clock_t(idle),
		(unsigned long long)nsec_to_clock_t(iowait),
		(unsigned long long)nsec_to_clock_t(irq),
		(unsigned long long)nsec_to_clock_t(softirq),
		(unsigned long long)nsec_to_clock_t(steal),
		(unsigned long long)nsec_to_clock_t(guest),
		(unsigned long long)nsec_to_clock_t(guest_nice));
	pr_info("-------------------------------------------------------------------------------------------------------------\n");

	for_each_possible_cpu(i) {
		/* Copy values here to work around gcc-2.95.3, gcc-2.96 */
		user	= kcpustat_cpu(i).cpustat[CPUTIME_USER];
		nice	= kcpustat_cpu(i).cpustat[CPUTIME_NICE];
		system	= kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle	= get_idle_time(i);
		iowait	= get_iowait_time(i);
		irq	= kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
		softirq	= kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
		steal	= kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
		guest	= kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
		guest_nice = kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];

		pr_info("cpu%d  user:%llu \tnice:%llu \tsystem:%llu \tidle:%llu \tiowait:%llu \tirq:%llu \tsoftirq:%llu \t %llu %llu %llu\n",
			i,
			(unsigned long long)nsec_to_clock_t(user),
			(unsigned long long)nsec_to_clock_t(nice),
			(unsigned long long)nsec_to_clock_t(system),
			(unsigned long long)nsec_to_clock_t(idle),
			(unsigned long long)nsec_to_clock_t(iowait),
			(unsigned long long)nsec_to_clock_t(irq),
			(unsigned long long)nsec_to_clock_t(softirq),
			(unsigned long long)nsec_to_clock_t(steal),
			(unsigned long long)nsec_to_clock_t(guest),
			(unsigned long long)nsec_to_clock_t(guest_nice));
	}
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info("irq : %llu", (unsigned long long)sum);
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	/* sum again ? it could be updated? */
	for_each_irq_nr(j) {
		unsigned int irq_stat = kstat_irqs(j);

		if (irq_stat) {
			pr_info("irq-%-4d : %8u %s\n", j, irq_stat,
				irq_to_desc(j)->action ? irq_to_desc(j)->action->name ? : "???" : "???");
		}
	}
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info("softirq : %llu", (unsigned long long)sum_softirq);
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
	for (i = 0; i < NR_SOFTIRQS; i++)
		if (per_softirq_sums[i])
			pr_info("softirq-%d : %8u %s\n", i, per_softirq_sums[i], softirq_to_name[i]);
	pr_info("-------------------------------------------------------------------------------------------------------------\n");
}

/* update magic for bootloader */
static void secdbg_base_set_upload_cause(enum sec_debug_upload_cause_t type)
{
	exynos_pmu_write(SEC_DEBUG_PANIC_INFORM, type);

	pr_emerg("sec_debug: set upload cause (0x%x)\n", type);
}

/* update recovery reason log */
static void secdbg_base_recovery_reboot(void)
{
#if 0
	char *buf;

	if (recovery_cause_offset) {
		if (!recovery_cause[0] || !strlen(recovery_cause)) {
			buf = "empty caller";
			store_recovery_cause(NULL, NULL, buf, strlen(buf));
		}
	}
#endif
}

/* Clear magic code in normal reboot */
void secdbg_base_clear_magic_rambase(void)
{
	secdbg_base_set_upload_magic(UPLOAD_MAGIC_INIT, NULL);
}

static int secdbg_base_reboot_handler(struct notifier_block *nb,
						unsigned long l, void *p)
{
	pr_emerg("sec_debug: %s\n", __func__);

	if (p != NULL)
		if (!strcmp(p, "recovery"))
			secdbg_base_recovery_reboot();

	return 0;
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = secdbg_base_reboot_handler
};

void secdbg_base_panic_handler(void *buf, bool dump)
{
	pr_emerg("sec_debug: %s\n", __func__);

	/* Set upload cause */
	secdbg_base_set_upload_magic(UPLOAD_MAGIC_PANIC, buf);
	if (!strncmp(buf, "User Fault", 10))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_USER_FAULT);
	else if (is_hard_reset_occurred())
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_HARD_RESET);
	else if (!strncmp(buf, "Crash Key", 9))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "User Crash Key", 14))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_USER_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_CP_ERROR_FATAL);
	else
		secdbg_base_set_upload_cause(UPLOAD_CAUSE_KERNEL_PANIC);

	/* dump debugging info */
	if (dump) {
		secdbg_base_dump_cpu_stat();
		debug_show_all_locks();
#ifdef CONFIG_SEC_DEBUG_COMPLETE_HINT
		secdbg_hint_display_complete_hint();
#endif
	}
}

void secdbg_base_post_panic_handler(void)
{
#if 0
	hard_reset_delay();
#endif

	/* reset */
	pr_emerg("sec_debug: %s\n", linux_banner);
	pr_emerg("sec_debug: rebooting...\n");

	flush_cache_all();
}

#ifdef CONFIG_SEC_DEBUG_TASK_IN_STATE_INFO
void secdbg_base_set_task_in_pm_suspend(uint64_t task)
{
	if (sdn)
		sdn->kernd.task_in_pm_suspend = task;
}

void secdbg_base_set_task_in_sys_reboot(uint64_t task)
{
	if (sdn)
		sdn->kernd.task_in_sys_reboot = task;
}
#endif /* SEC_DEBUG_TASK_IN_STATE_INFO */

struct watchdogd_info *secdbg_base_get_wdd_info(void)
{
	if (sdn) {
		pr_crit("%s: return right value\n", __func__);

		return &(sdn->kernd.wddinfo);
	}

	pr_crit("%s: return NULL\n", __func__);

	return NULL;
}

#ifdef CONFIG_VMAP_STACK
void secdbg_base_set_bs_info_phase(int phase)
{
	struct bad_stack_info *bsi;

	if (!sdn)
		return;

	bsi = &sdn->kernd.bsi;

	switch (phase) {
	case 2:
		bsi->irq_stk = (unsigned long)this_cpu_read(irq_stack_ptr);
		bsi->ovf_stk = (unsigned long)this_cpu_ptr(overflow_stack);
		bsi->tsk_stk = (unsigned long)current->stack;
		break;
	case 1:
	default:
		bsi->magic = 0xbad;
		bsi->spel0 = read_sysreg(sp_el0);
		bsi->esr = read_sysreg(esr_el1);
		bsi->far = read_sysreg(far_el1);
		bsi->cpu = raw_smp_processor_id();
		break;
	}
}
#endif

void *secdbg_base_get_debug_base(int type)
{
	if (sdn) {
		if (type == SDN_MAP_AUTO_COMMENT)
			return &(sdn->auto_comment);
		else if (type == SDN_MAP_EXTRA_INFO)
			return &(sdn->extra_info);
	}

	pr_crit("%s: return NULL\n", __func__);

	return NULL;
}

void *secdbg_base_get_kcnst_base(void)
{
	if (sdn)
		return &(sdn->kcnst);

	pr_crit("%s: return NULL\n", __func__);

	return NULL;
}

unsigned long secdbg_base_get_buf_base(int type)
{
	if (sdn) {
		pr_crit("%s: return %lx (%lx)\n", __func__, (unsigned long)sdn, sdn->map.buf[type].base);
		return sdn->map.buf[type].base;
	}

	pr_crit("%s: return 0\n", __func__);

	return 0;
}

unsigned long secdbg_base_get_buf_size(int type)
{
	if (sdn)
		return sdn->map.buf[type].size;

	pr_crit("%s: return 0\n", __func__);

	return 0;
}

void secdbg_base_write_buf(struct outbuf *obuf, int len, const char *fmt, ...)
{
	va_list list;
	char *base;
	int rem, ret;

	base = obuf->buf;
	base += obuf->index;

	rem = sizeof(obuf->buf);
	rem -= obuf->index;

	if (rem <= 0)
		return;

	if ((len > 0) && (len < rem))
		rem = len;

	va_start(list, fmt);
	ret = vsnprintf(base, rem, fmt, list);
	if (ret)
		obuf->index += ret;

	va_end(list);
}

#ifdef CONFIG_SEC_DEBUG_TASK_IN_STATE_INFO
void secdbg_base_set_task_in_sys_shutdown(uint64_t task)
{
	if (sdn)
		sdn->kernd.task_in_sys_shutdown = task;
}

void secdbg_base_set_task_in_dev_shutdown(uint64_t task)
{
	if (sdn)
		sdn->kernd.task_in_dev_shutdown = task;
}
#endif /* SEC_DEBUG_TASK_IN_STATE_INFO */

void secdbg_base_set_sysrq_crash(struct task_struct *task)
{
	if (!sdn)
		return;

	sdn->kernd.task_in_sysrq_crash = (uint64_t)task;

#ifdef CONFIG_SEC_DEBUG_SYSRQ_KMSG
	if (task) {
#if defined(CONFIG_SEC_DEBUG_INIT_LOG)
		if (strcmp(task->comm, "init") == 0)
			sdn->kernd.sysrq_ptr = secdbg_hook_get_curr_init_ptr();
		else
#endif
			sdn->kernd.sysrq_ptr = dbg_snapshot_get_curr_ptr_for_sysrq();

		pr_info("sysrq_ptr: 0x%lx\n", sdn->kernd.sysrq_ptr);
	}
#endif
}

void secdbg_base_set_task_in_soft_lockup(uint64_t task)
{
	if (sdn)
		sdn->kernd.task_in_soft_lockup = task;
}

void secdbg_base_set_cpu_in_soft_lockup(uint64_t cpu)
{
	if (sdn)
		sdn->kernd.cpu_in_soft_lockup = cpu;
}

void secdbg_base_set_task_in_hard_lockup(uint64_t task)
{
	if (sdn)
		sdn->kernd.task_in_hard_lockup = task;
}

void secdbg_base_set_cpu_in_hard_lockup(uint64_t cpu)
{
	if (sdn)
		sdn->kernd.cpu_in_hard_lockup = cpu;
}

#ifdef CONFIG_SEC_DEBUG_UNFROZEN_TASK
void secdbg_base_set_unfrozen_task(uint64_t task)
{
	if (sdn)
		sdn->kernd.unfrozen_task = task;
}

void secdbg_base_set_unfrozen_task_count(uint64_t count)
{
	if (sdn)
		sdn->kernd.unfrozen_task_count = count;
}
#endif /* SEC_DEBUG_UNFROZEN_TASK */

#ifdef CONFIG_SEC_DEBUG_TASK_IN_STATE_INFO
void secdbg_base_set_task_in_sync_irq(uint64_t task, unsigned int irq, const char *name, struct irq_desc *desc)
{
	if (sdn) {
		sdn->kernd.sync_irq_task = task;
		sdn->kernd.sync_irq_num = irq;
		sdn->kernd.sync_irq_name = (uint64_t)name;
		sdn->kernd.sync_irq_desc = (uint64_t)desc;

		if (desc) {
			sdn->kernd.sync_irq_threads_active = desc->threads_active.counter;

			if (desc->action && (desc->action->irq == irq) && desc->action->thread)
				sdn->kernd.sync_irq_thread = (uint64_t)(desc->action->thread);
			else
				sdn->kernd.sync_irq_thread = 0;
		}
	}
}
#endif /* SEC_DEBUG_TASK_IN_STATE_INFO */

#ifdef CONFIG_SEC_DEBUG_PM_DEVICE_INFO
void secdbg_base_set_device_shutdown_timeinfo(uint64_t start, uint64_t end, uint64_t duration, uint64_t func)
{
	if (sdn && func) {
		if (duration > sdn->kernd.dev_shutdown_duration) {
			sdn->kernd.dev_shutdown_start = start;
			sdn->kernd.dev_shutdown_end = end;
			sdn->kernd.dev_shutdown_duration = duration;
			sdn->kernd.dev_shutdown_func = func;
		}
	}
}

void secdbg_base_clr_device_shutdown_timeinfo(void)
{
	if (sdn) {
		sdn->kernd.dev_shutdown_start = 0;
		sdn->kernd.dev_shutdown_end = 0;
		sdn->kernd.dev_shutdown_duration = 0;
		sdn->kernd.dev_shutdown_func = 0;
	}
}

void secdbg_base_set_shutdown_device(const char *fname, const char *dname)
{
	if (sdn) {
		sdn->kernd.sdi.shutdown_func = (uint64_t)fname;
		sdn->kernd.sdi.shutdown_device = (uint64_t)dname;
	}
}

void secdbg_base_set_suspend_device(const char *fname, const char *dname)
{
	if (sdn) {
		sdn->kernd.sdi.suspend_func = (uint64_t)fname;
		sdn->kernd.sdi.suspend_device = (uint64_t)dname;
	}
}
#endif /* SEC_DEBUG_PM_DEVICE_INFO */

static void init_ess_info(unsigned int index, char *key)
{
	struct ess_info_offset *p;

	p = &(sdn->ss_info.item[index]);

	secdbg_base_get_kevent_info(p, index);

	memset(p->key, 0, SD_ESSINFO_KEY_SIZE);
	snprintf(p->key, SD_ESSINFO_KEY_SIZE, "%s", key);
}

/* initialize snapshot offset data in sec debug next */
static void secdbg_base_set_essinfo(void)
{
	unsigned int index = 0;

	memset(&(sdn->ss_info), 0, sizeof(struct sec_debug_ess_info));

	init_ess_info(index++, "kevnt-task");
	init_ess_info(index++, "kevnt-work");
	init_ess_info(index++, "kevnt-irq");
	init_ess_info(index++, "kevnt-freq");
	init_ess_info(index++, "kevnt-idle");
	init_ess_info(index++, "kevnt-thrm");
	init_ess_info(index++, "kevnt-acpm");
	init_ess_info(index++, "kevnt-mfrq");

	for (; index < SD_NR_ESSINFO_ITEMS;)
		init_ess_info(index++, "empty");

	for (index = 0; index < SD_NR_ESSINFO_ITEMS; index++)
		pr_info("%s: key: %s offset: %lx nr: %x\n", __func__,
				sdn->ss_info.item[index].key,
				sdn->ss_info.item[index].base,
				sdn->ss_info.item[index].nr);
}

/* initialize task_struct offset data in sec debug next */
static void secdbg_base_set_taskinfo(void)
{
	sdn->task.stack_size = THREAD_SIZE;
	sdn->task.start_sp = THREAD_START_SP;
	sdn->task.irq_stack.pcpu_stack = (uint64_t)&irq_stack_ptr;
	sdn->task.irq_stack.size = IRQ_STACK_SIZE;
	sdn->task.irq_stack.start_sp = IRQ_STACK_START_SP;

	sdn->task.ti.struct_size = sizeof(struct thread_info);
	SET_MEMBER_TYPE_INFO(&sdn->task.ti.flags, struct thread_info, flags);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.cpu, struct task_struct, cpu);

	sdn->task.ts.struct_size = sizeof(struct task_struct);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.state, struct task_struct, state);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.exit_state, struct task_struct,
					exit_state);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.stack, struct task_struct, stack);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.flags, struct task_struct, flags);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.on_cpu, struct task_struct, on_cpu);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.pid, struct task_struct, pid);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.on_rq, struct task_struct, on_rq);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.comm, struct task_struct, comm);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.tasks_next, struct task_struct,
					tasks.next);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.thread_group_next,
					struct task_struct, thread_group.next);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.fp, struct task_struct,
					thread.cpu_context.fp);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.sp, struct task_struct,
					thread.cpu_context.sp);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.pc, struct task_struct,
					thread.cpu_context.pc);

	SET_MEMBER_TYPE_INFO(&sdn->task.ts.sched_info__pcount,
					struct task_struct, sched_info.pcount);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.sched_info__run_delay,
					struct task_struct,
					sched_info.run_delay);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.sched_info__last_arrival,
					struct task_struct,
					sched_info.last_arrival);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.sched_info__last_queued,
					struct task_struct,
					sched_info.last_queued);
#ifdef CONFIG_SEC_DEBUG_DTASK
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.ssdbg_wait__type,
					struct task_struct,
					ssdbg_wait.type);
	SET_MEMBER_TYPE_INFO(&sdn->task.ts.ssdbg_wait__data,
					struct task_struct,
					ssdbg_wait.data);
#endif

	sdn->task.init_task = (uint64_t)&init_task;
}

static void secdbg_base_set_spinlockinfo(void)
{
#ifdef CONFIG_DEBUG_SPINLOCK
	SET_MEMBER_TYPE_INFO(&sdn->rlock.owner_cpu, struct raw_spinlock, owner_cpu);
	SET_MEMBER_TYPE_INFO(&sdn->rlock.owner, struct raw_spinlock, owner);
	sdn->rlock.debug_enabled = 1;
#else
	sdn->rlock.debug_enabled = 0;
#endif
}

/* initialize kernel constant data in sec debug next */
/* TODO: support this or not */
static unsigned long kconfig_base;
static unsigned long kconfig_size;

static void secdbg_base_set_kconstants(void)
{
	sdn->kcnst.nr_cpus = num_possible_cpus();
	sdn->kcnst.per_cpu_offset.pa = virt_to_phys(__per_cpu_offset);
	sdn->kcnst.per_cpu_offset.size = sizeof(__per_cpu_offset[0]);
	sdn->kcnst.per_cpu_offset.count = ARRAY_SIZE(__per_cpu_offset);

	sdn->kcnst.phys_offset = PHYS_OFFSET;
	sdn->kcnst.phys_mask = PHYS_MASK;
	sdn->kcnst.page_offset = PAGE_OFFSET;
	sdn->kcnst.page_mask = PAGE_MASK;
	sdn->kcnst.page_shift = PAGE_SHIFT;

	sdn->kcnst.va_bits = VA_BITS;
	sdn->kcnst.kimage_vaddr = kimage_vaddr;
	sdn->kcnst.kimage_voffset = kimage_voffset;

	sdn->kcnst.pa_swapper = (uint64_t)virt_to_phys(init_mm.pgd);
	sdn->kcnst.pgdir_shift = PGDIR_SHIFT;
	sdn->kcnst.pud_shift = PUD_SHIFT;
	sdn->kcnst.pmd_shift = PMD_SHIFT;
	sdn->kcnst.ptrs_per_pgd = PTRS_PER_PGD;
	sdn->kcnst.ptrs_per_pud = PTRS_PER_PUD;
	sdn->kcnst.ptrs_per_pmd = PTRS_PER_PMD;
	sdn->kcnst.ptrs_per_pte = PTRS_PER_PTE;

	sdn->kcnst.kconfig_base = kconfig_base;
	sdn->kcnst.kconfig_size = kconfig_size;

	sdn->kcnst.pa_text = virt_to_phys(_text);
	sdn->kcnst.pa_start_rodata = virt_to_phys(__start_rodata);

}

static void __init secdbg_base_init_sdn(struct sec_debug_next *d)
{
#define clear_sdn_field(__p, __m)	memset(&(__p)->__m, 0x0, sizeof((__p)->__m));

	clear_sdn_field(d, memtab);
	clear_sdn_field(d, ksyms);
	clear_sdn_field(d, kcnst);
	clear_sdn_field(d, task);
	clear_sdn_field(d, ss_info);
	clear_sdn_field(d, rlock);
	clear_sdn_field(d, kernd);
}

/* initialize sec debug next data structure */
static int __init secdbg_base_next_init(void)
{
	if (!sdn) {
		pr_info("%s: sdn is not allocated, quit\n", __func__);

		return -1;
	}

	pr_info("%s: start %lx (FROM LEGO)\n", __func__, (unsigned long)sdn);
	pr_info("%s: BUF_0: %lx\n", __func__, secdbg_base_get_buf_base(0));

	/* set magic */
	sdn->magic[0] = SEC_DEBUG_MAGIC0;
	sdn->magic[1] = SEC_DEBUG_MAGIC1;

	sdn->version[1] = SEC_DEBUG_KERNEL_UPPER_VERSION << 16;
	sdn->version[1] += SEC_DEBUG_KERNEL_LOWER_VERSION;

	/* set member table */
	secdbg_base_set_memtab_info(&sdn->memtab);

	/* set kernel symbols */
	secdbg_base_set_kallsyms_info(&(sdn->ksyms), SEC_DEBUG_MAGIC1);

	/* set kernel constants */
	secdbg_base_set_kconstants();
	secdbg_base_set_taskinfo();
	secdbg_base_set_essinfo();
	secdbg_base_set_spinlockinfo();

	secdbg_base_set_task_in_pm_suspend((uint64_t)NULL);
	secdbg_base_set_task_in_sys_reboot((uint64_t)NULL);
	secdbg_base_set_task_in_sys_shutdown((uint64_t)NULL);
	secdbg_base_set_task_in_dev_shutdown((uint64_t)NULL);
	secdbg_base_set_sysrq_crash(NULL);
	secdbg_base_set_task_in_soft_lockup((uint64_t)NULL);
	secdbg_base_set_cpu_in_soft_lockup((uint64_t)0);
	secdbg_base_set_task_in_hard_lockup((uint64_t)NULL);
	secdbg_base_set_cpu_in_hard_lockup((uint64_t)0);
	secdbg_base_set_unfrozen_task((uint64_t)NULL);
	secdbg_base_set_unfrozen_task_count((uint64_t)0);
	secdbg_base_set_task_in_sync_irq((uint64_t)NULL, 0, NULL, NULL);
	secdbg_base_clr_device_shutdown_timeinfo();

	pr_info("%s: done\n", __func__);

	return 0;
}

/* reserve first 1 page from dram base (generally 0x80000000) */
static int __init secdbg_base_set_magic(struct reserved_mem *rmem)
{
	pr_info("%s: Reserved Mem(0x%llx, 0x%llx) - Success\n",
		__func__, rmem->base, rmem->size);

	secdbg_base_rmem_phys = rmem->base;
	secdbg_base_rmem_size = rmem->size;

	return 0;
}
RESERVEDMEM_OF_DECLARE(sec_debug_magic, "exynos,sec_debug_magic", secdbg_base_set_magic);

/* set first 1 page from dram base (generally 0x80000000) as non-cacheable */
static unsigned long __init __set_mem_as_nocache(unsigned long base, unsigned long size, int default_clear)
{
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	int page_size, i;
	struct page *page;
	struct page **pages;
	void *addr;
	unsigned long ret;

	if (!size || !base) {
		pr_err("%s: failed to set nocache pages\n", __func__);

		return 0;
	}

	page_size = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	pages = kcalloc(page_size, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_err("%s: failed to allocate pages\n", __func__);

		return 0;
	}

	page = phys_to_page(base);
	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	addr = vm_map_ram(pages, page_size, -1, prot);
	if (!addr) {
		pr_err("%s: failed to mapping between virt and phys\n", __func__);
		kfree(pages);

		return 0;
	}

	ret = (unsigned long)addr;
	pr_info("%s: virt: 0x%lx\n", __func__, ret);

	kfree(pages);

	if (default_clear) {
		pr_info("%s: clear this area\n", __func__);
		memset(addr, 0, size);
	}

	return ret;
}

static int __init secdbg_base_nocache_remap(void)
{
	secdbg_base_rmem_virt = __set_mem_as_nocache(secdbg_base_rmem_phys, secdbg_base_rmem_size, 1);
	sdn = (struct sec_debug_next *)__set_mem_as_nocache(secdbg_sdn_phys, secdbg_sdn_size, 0);

	secdbg_base_next_init();
	secdbg_base_set_upload_magic(UPLOAD_MAGIC_PANIC, NULL);
	register_reboot_notifier(&nb_reboot_block);

	return 0;
}
early_initcall(secdbg_base_nocache_remap);

/* get sec debug next start address and memory size from bootloader */
static int __init secdbg_base_reserve_memory(char *str)
{
	unsigned long size = memparse(str, &str);
	unsigned long base = 0;

	/* If we encounter any problem parsing str ... */
	if (!size || *str != '@' || kstrtoul(str + 1, 0, &base)) {
		pr_err("%s: failed to parse address.\n", __func__);
		goto out;
	}

#ifdef CONFIG_NO_BOOTMEM
	if (memblock_is_region_reserved(base, size) || memblock_reserve(base, size)) {
#else
	if (reserve_bootmem(base, size, BOOTMEM_EXCLUSIVE)) {
#endif
		/* size is not match with -size and size + sizeof(...) */
		pr_err("%s: failed to reserve size:0x%lx at base 0x%lx\n",
		       __func__, size, base);
		goto out;
	}

	sdn = (struct sec_debug_next *)phys_to_virt(base);
	if (!sdn) {
		pr_info("%s: fail to init sec debug next buffer\n", __func__);

		goto out;
	}

	secdbg_sdn_phys = base;
	secdbg_sdn_size = size;

	secdbg_base_init_sdn(sdn);

	pr_info("%s: base(virt):0x%lx size:0x%lx\n", __func__,
				(unsigned long)sdn, size);
	pr_info("%s: ds size: 0x%lx\n", __func__,
				round_up(sizeof(struct sec_debug_next), PAGE_SIZE));

out:
	return 0;
}
__setup("sec_debug_next=", secdbg_base_reserve_memory);

static int __init secdbg_base_setup_kernel_controls(void)
{
	if (IS_ENABLED(CONFIG_SEC_DEBUG_PANIC_ON_RCU_STALL)) {
		pr_info("set panic_on_rcu_stall\n");
		sysctl_panic_on_rcu_stall = 1;
	}

	return 0;
}
subsys_initcall(secdbg_base_setup_kernel_controls);
