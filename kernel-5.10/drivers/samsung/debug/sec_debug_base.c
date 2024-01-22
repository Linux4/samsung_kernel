/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/cacheflush.h>
#include <asm/io.h>
#include <asm/stacktrace.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/kexec.h>
#include <linux/kmsg_dump.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ptrace.h>
#include <linux/reboot.h>
#include <linux/tick.h>
#include <linux/uaccess.h>
#include <linux/sched/mm.h>
#include <linux/sched/debug.h>
#include <linux/nmi.h>
#include <linux/sec_debug.h>
#include <linux/platform_device.h>
#include <linux/of_reserved_mem.h>

/* get time function to log */
#ifndef arch_irq_stat_cpu
#define arch_irq_stat_cpu(cpu) 0
#endif
#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

static unsigned int __read_mostly debug_level;
module_param(debug_level, uint, 0440);

unsigned int __read_mostly reset_reason;
module_param(reset_reason, uint, 0440);

DEFINE_PER_CPU(unsigned char, coreregs_stored);
DEFINE_PER_CPU(struct pt_regs, sec_aarch64_core_reg);
DEFINE_PER_CPU(sec_debug_mmu_reg_t, sec_aarch64_mmu_reg);

void sec_debug_save_core_reg(void *v_regs)
{
	struct pt_regs *regs = (struct pt_regs *)v_regs;
	struct pt_regs *core_reg =
			&per_cpu(sec_aarch64_core_reg, smp_processor_id());

	if (!regs) {
		asm volatile (
			"stp x0, x1, [%0, #8 * 0]\n\t"
			"stp x2, x3, [%0, #8 * 2]\n\t"
			"stp x4, x5, [%0, #8 * 4]\n\t"
			"stp x6, x7, [%0, #8 * 6]\n\t"
			"stp x8, x9, [%0, #8 * 8]\n\t"
			"stp x10, x11, [%0, #8 * 10]\n\t"
			"stp x12, x13, [%0, #8 * 12]\n\t"
			"stp x14, x15, [%0, #8 * 14]\n\t"
			"stp x16, x17, [%0, #8 * 16]\n\t"
			"stp x18, x19, [%0, #8 * 18]\n\t"
			"stp x20, x21, [%0, #8 * 20]\n\t"
			"stp x22, x23, [%0, #8 * 22]\n\t"
			"stp x24, x25, [%0, #8 * 24]\n\t"
			"stp x26, x27, [%0, #8 * 26]\n\t"
			"stp x28, x29, [%0, #8 * 28]\n\t"
			"stp x5, x6, [sp,#-32]!\n\t"
			"str x7, [sp,#16]\n\t"
			"mov x5, sp\n\t"
			"stp x30, x5, [%0, #8 * 30]\n\t"
			"adr x6, 1f\n\t"
			"mrs x7, spsr_el1\n\t"
			"stp x6, x7, [%0, #8 * 32]\n\t"
			"ldr x7, [sp,#16]\n\t"
			"ldp x5, x6, [sp],#32\n\t"
		"1:"
			:
			: "r" (&core_reg->regs[0])
			: "memory"
		);
	} else {
		memcpy(core_reg, regs, sizeof(struct pt_regs));
	}

	return;
}

void sec_debug_save_mmu_reg(sec_debug_mmu_reg_t *mmu_reg)
{
	asm("mrs x1, SCTLR_EL1\n\t"		/* SCTLR_EL1 */
	    "str x1, [%0]\n\t"
	    "mrs x1, TTBR0_EL1\n\t"		/* TTBR0_EL1 */
	    "str x1, [%0,#8]\n\t"
	    "mrs x1, TTBR1_EL1\n\t"		/* TTBR1_EL1 */
	    "str x1, [%0,#16]\n\t"
	    "mrs x1, TCR_EL1\n\t"		/* TCR_EL1 */
	    "str x1, [%0,#24]\n\t"
	    "mrs x1, ESR_EL1\n\t"		/* ESR_EL1 */
	    "str x1, [%0,#32]\n\t"
	    "mrs x1, FAR_EL1\n\t"		/* FAR_EL1 */
	    "str x1, [%0,#40]\n\t"
	    /* Don't populate AFSR0_EL1 and AFSR1_EL1 */
	    "mrs x1, CONTEXTIDR_EL1\n\t"	/* CONTEXTIDR_EL1 */
	    "str x1, [%0,#48]\n\t"
	    "mrs x1, TPIDR_EL0\n\t"		/* TPIDR_EL0 */
	    "str x1, [%0,#56]\n\t"
	    "mrs x1, TPIDRRO_EL0\n\t"		/* TPIDRRO_EL0 */
	    "str x1, [%0,#64]\n\t"
	    "mrs x1, TPIDR_EL1\n\t"		/* TPIDR_EL1 */
	    "str x1, [%0,#72]\n\t"
	    "mrs x1, MAIR_EL1\n\t"		/* MAIR_EL1 */
	    "str x1, [%0,#80]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%x1", "memory"			/* clobbered register */
	);
}


void sec_debug_init_mmu(void *unused)
{
	sec_debug_save_mmu_reg(&per_cpu(sec_aarch64_mmu_reg, raw_smp_processor_id()));
}

int sec_save_context(int cpu, void *v_regs)
{
	unsigned long flags;
	struct pt_regs *regs = (struct pt_regs *)v_regs;
	//struct pt_regs tmp;

	if (cpu == _THIS_CPU) {
		if (__this_cpu_read(coreregs_stored)) {
			pr_emerg("skip context saved(CPU:%d)\n", smp_processor_id());
			return 0;
		}
	} else {
		if (per_cpu(coreregs_stored, cpu)) {
			pr_emerg("skip context saved(CPU:%d)\n", cpu);
			return 0;
		}
	}

	local_irq_save(flags);
	sec_debug_save_core_reg(regs);

	if (cpu == _THIS_CPU) {
		sec_debug_save_mmu_reg(&per_cpu(sec_aarch64_mmu_reg, smp_processor_id()));
	} else {
		sec_debug_save_mmu_reg(&per_cpu(sec_aarch64_mmu_reg, cpu));
	}

	//crash_setup_regs(&tmp, regs);
	//crash_save_vmcoreinfo();
	if (cpu == _THIS_CPU) {
		//crash_save_cpu(&tmp, smp_processor_id());
		__this_cpu_inc(coreregs_stored);
		pr_emerg("context saved(CPU:%d)\n", smp_processor_id());
	} else {
		//crash_save_cpu(&tmp, cpu);
		++per_cpu(coreregs_stored, cpu);
		pr_emerg("context saved(CPU:%d)\n", cpu);
	}

	/* TO_DO : check cache flush api */
	//__inner_flush_dcache_all();

	local_irq_restore(flags);

	return 0;
}

static void secdbg_show_debug_level(void)
{
	pr_info("%s: debug_level=%d\n", __func__, debug_level);
}

bool is_debug_level_low(void)
{
	if(debug_level == 0) {
		pr_info("%s: YES\n", __func__);	
		return true;
	}

	pr_info("%s: NO, debug_level=%d\n", __func__, debug_level);	
	return false;
}
EXPORT_SYMBOL(is_debug_level_low);

#define task_contributes_to_load(task)  ((task->state & TASK_UNINTERRUPTIBLE) != 0 && \
		(task->flags & PF_FROZEN) == 0 && \
		(task->state & TASK_NOLOAD) == 0)

static unsigned long secdbg_get_wchan(struct task_struct *p)
{
	unsigned long entry = 0;
	unsigned int skip = 0;

	if (p != current)
		skip = 1;	/* to skip __switch_to */

	stack_trace_save_tsk(p, &entry, 1, skip);

	return entry;
}

static void sec_dump_one_task_info(struct task_struct *tsk, bool is_main)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'X', 'Z', 'P', 'x', 'K', 'W', 'I', 'N'};
	unsigned char idx = 0;
	unsigned long state;
	unsigned long wchan;
	unsigned long pc = 0;
	char symname[KSYM_NAME_LEN];

	if ((tsk == NULL) || !try_get_task_stack(tsk))
		return;

	state = tsk->state | tsk->exit_state;
	pc = KSTK_EIP(tsk);

	while (state) {
		idx++;
		state >>= 1;
	}

	wchan = secdbg_get_wchan(tsk);
	snprintf(symname, KSYM_NAME_LEN, "%ps", wchan);

	touch_softlockup_watchdog();

  	pr_info("%8d %8d %8d %16lld %c(%d) %3d  %16lx %16lx  %16lx %c %16s [%s]\n",
  			tsk->pid, (int)(tsk->utime), (int)(tsk->stime),
  			tsk->se.exec_start, state_array[idx], (int)(tsk->state),
  			task_cpu(tsk), wchan, pc, (unsigned long)tsk,
  			is_main ? '*' : ' ', tsk->comm, symname);

	if (tsk->on_cpu && tsk->on_rq && tsk->cpu != smp_processor_id())
		return;

	if (tsk->state == TASK_RUNNING || tsk->state == TASK_WAKING ||
					task_contributes_to_load(tsk)) {
		dump_backtrace(NULL, tsk, KERN_DEFAULT);
	}
}

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next,
				struct task_struct,
				thread_group);
}

static void sec_dump_task_info(void)
{
	struct task_struct *frst_tsk;
	struct task_struct *curr_tsk;
	struct task_struct *frst_thr;
	struct task_struct *curr_thr;

	pr_info("\n");
	pr_info(" current proc : %d %s\n", current->pid, current->comm);
	pr_info(" ----------------------------------------------------------------------------------------------------------------------------\n");
	pr_info("     pid  uTime(ms)  sTime(ms)    exec(ns)  stat  cpu       wchan           user_pc        task_struct       comm   sym_wchan\n");
	pr_info(" ----------------------------------------------------------------------------------------------------------------------------\n");

	/* processes */
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while (curr_tsk != NULL) {
		sec_dump_one_task_info(curr_tsk,  true);
		/* threads */
		if (curr_tsk->thread_group.next != NULL) {
			frst_thr = get_next_thread(curr_tsk);
			curr_thr = frst_thr;
			if (frst_thr != curr_tsk) {
				while (curr_thr != NULL) {
					sec_dump_one_task_info(curr_thr, false);
					curr_thr = get_next_thread(curr_thr);
					if (curr_thr == curr_tsk)
						break;
				}
			}
		}
		curr_tsk = container_of(curr_tsk->tasks.next,
					struct task_struct, tasks);
		if (curr_tsk == frst_tsk)
			break;
	}
	pr_info(" ----------------------------------------------------------------------------------------------------------------------------\n");
}

static void sec_dump_irq_info(void)
{
	int i, cpu;
	u64 sum = 0;

	for_each_possible_cpu(cpu)
		sum += kstat_cpu_irqs_sum(cpu);

	sum += arch_irq_stat();

	pr_info("<irq info>\n");
	pr_info("----------------------------------------------------------\n");
	pr_info("sum irq : %llu", sum);
	pr_info("----------------------------------------------------------\n");

	for_each_irq_nr(i) {
		struct irq_desc *desc = irq_to_desc(i);
		unsigned int irq_stat = 0;
		const char *name;

		if (!desc)
			continue;

		for_each_possible_cpu(cpu)
				irq_stat += *per_cpu_ptr(desc->kstat_irqs, cpu);

		if (!irq_stat)
			continue;

		if (desc->action && desc->action->name)
			name = desc->action->name;
		else
			name = "???";
		pr_info("irq-%-4d(hwirq-%-3d) : %8u %s\n",
			i, (int)desc->irq_data.hwirq, irq_stat, name);
	}
}

void sec_debug_dump_info(struct pt_regs *regs)
{
	sec_save_context(_THIS_CPU, regs);
	sec_dump_task_info();
	sec_dump_irq_info();
}
EXPORT_SYMBOL(sec_debug_dump_info);

static int set_reset_reason_proc_show(struct seq_file *m, void *v)
{
	if (reset_reason == RR_S)
		seq_puts(m, "SPON\n");
	else if (reset_reason == RR_W)
		seq_puts(m, "WPON\n");
	else if (reset_reason == RR_D)
		seq_puts(m, "DPON\n");
	else if (reset_reason == RR_K)
		seq_puts(m, "KPON\n");
	else if (reset_reason == RR_M)
		seq_puts(m, "MPON\n");
	else if (reset_reason == RR_P)
		seq_puts(m, "PPON\n");
	else if (reset_reason == RR_R)
		seq_puts(m, "RPON\n");
	else if (reset_reason == RR_B)
		seq_puts(m, "BPON\n");
	else if (reset_reason == RR_T)
		seq_puts(m, "TPON\n");
	else if (reset_reason == RR_C)
		seq_puts(m, "CPON\n");
	else
		seq_puts(m, "NPON\n");

	return 0;
}

static int sec_reset_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_reset_reason_proc_show, NULL);
}

static const struct proc_ops sec_reset_reason_proc_fops = {
	.proc_open = sec_reset_reason_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int sec_debug_reset_reason_store_lastkmsg_proc_show(struct seq_file *m, void *v)
{
	if (reset_reason == RR_K || reset_reason == RR_D || reset_reason == RR_P)
		seq_puts(m, "1\n");
	else
		seq_puts(m, "0\n");

	return 0;
}

static int sec_debug_reset_reason_store_lastkmsg_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_debug_reset_reason_store_lastkmsg_proc_show, NULL);
}

static const struct proc_ops sec_debug_reset_reason_store_lastkmsg_proc_fops = {
	.proc_open = sec_debug_reset_reason_store_lastkmsg_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int sec_debug_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason", 0400, NULL,
		&sec_reset_reason_proc_fops);

	if (!entry)
		return -ENOMEM;

	entry = proc_create("store_lastkmsg", 0400, NULL,
		&sec_debug_reset_reason_store_lastkmsg_proc_fops);

	if (!entry)
		return -ENOMEM;

	return 0;
}

struct platform_device *secdbg_pdev;
EXPORT_SYMBOL(secdbg_pdev);

extern int sec_debug_log_init(void);
extern int secdbg_init_log_init(void);
extern int secdbg_pmsg_log_init(void);
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
extern int sec_debug_auto_comment_init(void);
#endif
#if IS_ENABLED(CONFIG_SEC_DEBUG_HISTORY_LOG)
extern int sec_debug_history_init(void);
#endif
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
//extern int secdbg_extra_info_init(void);
//extern int secdbg_hw_param_init(void);
#endif

static int sec_debug_probe(struct platform_device *pdev)
{
	pr_info("%s: start\n", __func__);

	secdbg_pdev = pdev;

	sec_debug_log_init();
	secdbg_init_log_init();
	secdbg_pmsg_log_init();

	sec_debug_reset_reason_init();

#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
	sec_debug_auto_comment_init();
#endif
#if IS_ENABLED(CONFIG_SEC_DEBUG_HISTORY_LOG)
	sec_debug_history_init();
#endif
#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	//secdbg_extra_info_init();
	//secdbg_hw_param_init();
#endif

	smp_call_function(sec_debug_init_mmu, NULL, 1);
	sec_debug_init_mmu(NULL);

	secdbg_show_debug_level();

	return 0;
}

static const struct of_device_id sec_debug_of_match[] = {
	{ .compatible	= "samsung,sec_debug" },
	{},
};
MODULE_DEVICE_TABLE(of, sec_debug_of_match);

static struct platform_driver sec_debug_driver = {
	.probe = sec_debug_probe,
	.driver  = {
		.name  = "sec_debug_base",
		.of_match_table = of_match_ptr(sec_debug_of_match),
	},
};

static int __init sec_debug_init(void)
{
	return platform_driver_register(&sec_debug_driver);
}
core_initcall(sec_debug_init);

static void __exit sec_debug_exit(void)
{
	platform_driver_unregister(&sec_debug_driver);
}
module_exit(sec_debug_exit);

MODULE_DESCRIPTION("Samsung Debug driver");
MODULE_LICENSE("GPL v2");
