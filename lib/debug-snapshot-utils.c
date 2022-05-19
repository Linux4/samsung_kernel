/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Debug-SnapShot: Debug Framework for Ramdump based debugging method
 * The original code is Exynos-Snapshot for Exynos SoC
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 * Author: Changki Kim <changki.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>
#include <linux/input.h>
#include <linux/smc.h>
#include <linux/bitops.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/nmi.h>
#include <linux/init_task.h>
#include <linux/ftrace.h>
#include <linux/sec_debug.h>

#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/core_regs.h>
#include <asm/cacheflush.h>
#include <linux/irqflags.h>

#include "debug-snapshot-local.h"

DEFINE_PER_CPU(struct pt_regs *, dss_core_reg);
DEFINE_PER_CPU(struct dbg_snapshot_mmu_reg *, dss_mmu_reg);

struct dbg_snapshot_allcorelockup_param {
	unsigned long last_pc_addr;
	unsigned long spin_pc_addr;
} dss_allcorelockup_param;

void dbg_snapshot_hook_hardlockup_entry(void *v_regs)
{
	int cpu = raw_smp_processor_id();
	unsigned int val;

	if (!dbg_snapshot_get_enable())
		return;

	if (!dss_desc.hardlockup_core_mask) {
		if (dss_desc.multistage_wdt_irq &&
			!dss_desc.allcorelockup_detected) {
			/* 1st FIQ trigger */
			val = readl(dbg_snapshot_get_base_vaddr() +
				DSS_OFFSET_CORE_LAST_PC + (DSS_NR_CPUS * sizeof(unsigned long)));
			if (val == DSS_SIGN_LOCKUP || val == (DSS_SIGN_LOCKUP + 1)) {
				dss_desc.allcorelockup_detected = true;
				dss_desc.hardlockup_core_mask = GENMASK(DSS_NR_CPUS - 1, 0);
			} else {
				return;
			}
		}
	}

	/* re-check the cpu number which is lockup */
	if (dss_desc.hardlockup_core_mask & BIT(cpu)) {
		int ret;
		unsigned long last_pc;
		struct pt_regs *regs;
		unsigned long timeout = USEC_PER_SEC * 2;

		do {
			/*
			 * If one cpu is occurred to lockup,
			 * others are going to output its own information
			 * without side-effect.
			 */
			ret = do_raw_spin_trylock(&dss_desc.nmi_lock);
			if (!ret)
				udelay(1);
		} while (!ret && timeout--);

		last_pc = dbg_snapshot_get_last_pc(cpu);

		regs = (struct pt_regs *)v_regs;

		/* Replace real pc value even if it is invalid */
		regs->pc = last_pc;

		/* Then, we expect bug() function works well */
		dev_emerg(dss_desc.dev, "\n------------------------------------------------------------------------------\n"
					"%s - Debugging Information for Hardlockup core(%d) - locked CPUs %*pbl"
					"\n------------------------------------------------------------------------------\n\n",
					(dss_desc.allcorelockup_detected) ? "All Core" : "Core", cpu,
					cpumask_pr_args((cpumask_t *)&dss_desc.hardlockup_core_mask));

#if defined(CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU) && defined(CONFIG_SEC_DEBUG_LOCKUP_INFO)
		update_hardlockup_type(cpu);
#endif
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
		secdbg_exin_set_backtrace_cpu(v_regs, cpu);
#endif
	}
}

void dbg_snapshot_hook_hardlockup_exit(void)
{
	int cpu = raw_smp_processor_id();

	if (!dbg_snapshot_get_enable() ||
		!dss_desc.hardlockup_core_mask) {
		return;
	}

	/* re-check the cpu number which is lockup */
	if (dss_desc.hardlockup_core_mask & BIT(cpu)) {
		/* clear bit to complete replace */
		dss_desc.hardlockup_core_mask &= ~(BIT(cpu));
		/*
		 * If this unlock function does not make a side-effect
		 * even it's not lock
		 */
		do_raw_spin_unlock(&dss_desc.nmi_lock);
	}
}

void dbg_snapshot_recall_hardlockup_core(void)
{
	int i;
#ifdef SMC_CMD_KERNEL_PANIC_NOTICE
	int ret;
#endif
	unsigned long cpu_mask = 0, tmp_bit = 0;
	unsigned long last_pc_addr = 0, timeout;

	if (!dbg_snapshot_get_enable())
		goto out;

	if (dss_desc.allcorelockup_detected) {
		dev_emerg(dss_desc.dev, "debug-snapshot: skip recall hardlockup for dump of each core\n");
		goto out;
	}

	for (i = 0; i < DSS_NR_CPUS; i++) {
		if (i == raw_smp_processor_id())
			continue;
		tmp_bit = cpu_online_mask->bits[DSS_NR_CPUS/SZ_64] & (1 << i);
		if (tmp_bit)
			cpu_mask |= tmp_bit;
	}

	if (!cpu_mask)
		goto out;

	last_pc_addr = dbg_snapshot_get_last_pc_paddr();

	dev_emerg(dss_desc.dev, "debug-snapshot: core hardlockup mask information: 0x%lx\n", cpu_mask);
	dss_desc.hardlockup_core_mask = cpu_mask;

#ifdef SMC_CMD_KERNEL_PANIC_NOTICE
	/* Setup for generating NMI interrupt to unstopped CPUs */
	ret = dss_soc_ops->soc_smc_call(SMC_CMD_KERNEL_PANIC_NOTICE,
				 cpu_mask,
				 (unsigned long)dbg_snapshot_bug_func,
				 last_pc_addr);
	if (ret) {
		dev_emerg(dss_desc.dev, "debug-snapshot: failed to generate NMI, "
			 "not support to dump information of core\n");
		dss_desc.hardlockup_core_mask = 0;
		goto out;
	}
#endif
	/* Wait up to 3 seconds for NMI interrupt */
	timeout = USEC_PER_SEC * 3;
	while (dss_desc.hardlockup_core_mask != 0 && timeout--)
		udelay(1);
out:
	return;
}

void dbg_snapshot_save_system(void *unused)
{
	struct dbg_snapshot_mmu_reg *mmu_reg;

	if (!dbg_snapshot_get_enable())
		return;

	mmu_reg = per_cpu(dss_mmu_reg, raw_smp_processor_id());

	dss_soc_ops->soc_save_system((void *)mmu_reg);
}

int dbg_snapshot_dump(void)
{
	dss_soc_ops->soc_dump_info(NULL);
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_dump);

int dbg_snapshot_save_core(void *v_regs)
{
	struct pt_regs *regs = (struct pt_regs *)v_regs;
	struct pt_regs *core_reg =
			per_cpu(dss_core_reg, smp_processor_id());

	if (!dbg_snapshot_get_enable())
		return 0;

	if (!regs)
		dss_soc_ops->soc_save_core((void *)core_reg);
	else
		memcpy(core_reg, regs, sizeof(struct user_pt_regs));

	dev_emerg(dss_desc.dev, "debug-snapshot: core register saved(CPU:%d)\n",
						smp_processor_id());
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_save_core);

int dbg_snapshot_save_context(void *v_regs)
{
	int cpu;
	unsigned long flags;
	struct pt_regs *regs = (struct pt_regs *)v_regs;

	if (!dbg_snapshot_get_enable())
		return 0;

	dss_soc_ops->soc_save_context_entry(NULL);

	cpu = smp_processor_id();
	raw_spin_lock_irqsave(&dss_desc.ctrl_lock, flags);

	/* If it was already saved the context information, it should be skipped */
	if (dbg_snapshot_get_core_panic_stat(cpu) !=  DSS_SIGN_PANIC) {
		dbg_snapshot_save_system(NULL);
		dbg_snapshot_save_core(regs);
		dbg_snapshot_dump();
		dbg_snapshot_set_core_panic_stat(DSS_SIGN_PANIC, cpu);
		dev_emerg(dss_desc.dev, "debug-snapshot: context saved(CPU:%d)\n", cpu);
	} else
		dev_emerg(dss_desc.dev, "debug-snapshot: skip context saved(CPU:%d)\n", cpu);

	raw_spin_unlock_irqrestore(&dss_desc.ctrl_lock, flags);

	dss_soc_ops->soc_save_context_exit(NULL);
	return 0;
}
EXPORT_SYMBOL(dbg_snapshot_save_context);

static bool __dbg_snapshot_is_dump_backtrace(const char *wchan_name)
{
	static bool printed_once = false;

	if (strncmp(wchan_name, "process_notifier", 17))
		return true;

	if (printed_once)
		return false;

	printed_once = true;
	return true;
}

static void dbg_snapshot_dump_one_task_info(struct task_struct *tsk, bool is_main)
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
	wchan = get_wchan(tsk);
	if (lookup_symbol_name(wchan, symname) < 0)
		snprintf(symname, KSYM_NAME_LEN, "%lu", wchan);

	while (state) {
		idx++;
		state >>= 1;
	}

	/*
	 * kick watchdog to prevent unexpected reset during panic sequence
	 * and it prevents the hang during panic sequence by watchedog
	 */
	touch_softlockup_watchdog();
	dss_soc_ops->soc_kick_watchdog(NULL);

	dev_info(dss_desc.dev, "%8d %8llu %8llu %16llu %c(%d) %3d  %16zx %16zx  %16zx %c %16s [%s]\n",
			tsk->pid, tsk->utime / NSEC_PER_MSEC, tsk->stime / NSEC_PER_MSEC,
			tsk->se.exec_start, state_array[idx], (int)(tsk->state),
			task_cpu(tsk), wchan, pc, (unsigned long)tsk,
			is_main ? '*' : ' ', tsk->comm, symname);

	if (tsk->state == TASK_RUNNING ||
	    tsk->state == TASK_WAKING ||
	    task_contributes_to_load(tsk)) {
		secdbg_dtsk_print_info(tsk, true);

		if (tsk->on_cpu && tsk->on_rq &&
		    tsk->cpu != smp_processor_id())
			return;

		if (__dbg_snapshot_is_dump_backtrace(symname)) {
			show_stack(tsk, NULL);
			dev_info(dss_desc.dev, "\n");
		}
	}
}

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next,
				struct task_struct,
				thread_group);
}

void dbg_snapshot_dump_task_info(void)
{
	struct task_struct *frst_tsk;
	struct task_struct *curr_tsk;
	struct task_struct *frst_thr;
	struct task_struct *curr_thr;

	if (!dbg_snapshot_get_enable())
		return;

	dev_info(dss_desc.dev, "\n");
	dev_info(dss_desc.dev, " current proc : %d %s\n", current->pid, current->comm);
	dev_info(dss_desc.dev, " ----------------------------------------------------------------------------------------------------------------------------\n");
	dev_info(dss_desc.dev, "     pid  uTime(ms)  sTime(ms)    exec(ns)  stat  cpu       wchan           user_pc        task_struct       comm   sym_wchan\n");
	dev_info(dss_desc.dev, " ----------------------------------------------------------------------------------------------------------------------------\n");

	/* processes */
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while (curr_tsk != NULL) {
		dbg_snapshot_dump_one_task_info(curr_tsk,  true);
		/* threads */
		if (curr_tsk->thread_group.next != NULL) {
			frst_thr = get_next_thread(curr_tsk);
			curr_thr = frst_thr;
			if (frst_thr != curr_tsk) {
				while (curr_thr != NULL) {
					dbg_snapshot_dump_one_task_info(curr_thr, false);
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
	dev_info(dss_desc.dev, " ----------------------------------------------------------------------------------------------------------------------------\n");
}

void dbg_snapshot_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	static const unsigned int VOLUME_UP = KEY_VOLUMEUP;
	static const unsigned int VOLUME_DOWN = KEY_VOLUMEDOWN;

	if (code == KEY_POWER)
		dev_info(dss_desc.dev, "debug-snapshot: POWER-KEY %s\n", value ? "pressed" : "released");

	/* Enter Forced Upload
	 *  Hold volume down key first
	 *  and then press power key twice
	 *  and volume up key should not be pressed
	 */
	if (value) {
		if (code == VOLUME_UP)
			volup_p = true;
		if (code == VOLUME_DOWN)
			voldown_p = true;
		if (!volup_p && voldown_p) {
			if (code == KEY_POWER) {
				dev_info
				    (dss_desc.dev, "debug-snapshot: count for entering forced upload [%d]\n",
				     ++loopcount);
				if (loopcount == 2) {
					panic("Crash Key");
				}
			}
		}
	} else {
		if (code == VOLUME_UP)
			volup_p = false;
		if (code == VOLUME_DOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}
EXPORT_SYMBOL(dbg_snapshot_check_crash_key);

void __init dbg_snapshot_allcorelockup_detector_init(void)
{
	int ret = -1;

	if (!dss_desc.multistage_wdt_irq)
		return;

	dss_allcorelockup_param.last_pc_addr = dbg_snapshot_get_last_pc_paddr();
	dss_allcorelockup_param.spin_pc_addr = __pa_symbol(dbg_snapshot_spin_func);

	__flush_dcache_area((void *)&dss_allcorelockup_param,
			sizeof(struct dbg_snapshot_allcorelockup_param));

#ifdef SMC_CMD_LOCKUP_NOTICE
	/* Setup for generating NMI interrupt to unstopped CPUs */
	ret = dss_soc_ops->soc_smc_call(SMC_CMD_LOCKUP_NOTICE,
				 (unsigned long)dbg_snapshot_bug_func,
				 dss_desc.multistage_wdt_irq,
				 (unsigned long)(virt_to_phys)(&dss_allcorelockup_param));
#endif

	dev_emerg(dss_desc.dev, "debug-snapshot: %s to register all-core lockup detector - ret: %d\n",
			ret == 0 ? "success" : "failed", ret);
}

void __init dbg_snapshot_init_utils(void)
{
	size_t vaddr;
	int i;

	vaddr = dss_items[DSS_ITEM_HEADER_ID].entry.vaddr;

	for (i = 0; i < DSS_NR_CPUS; i++) {
		per_cpu(dss_mmu_reg, i) = (struct dbg_snapshot_mmu_reg *)
					  (vaddr + DSS_HEADER_SZ +
					   i * DSS_MMU_REG_OFFSET);
		per_cpu(dss_core_reg, i) = (struct pt_regs *)
					   (vaddr + DSS_HEADER_SZ + DSS_MMU_REG_SZ +
					    i * DSS_CORE_REG_OFFSET);
	}

	/* hardlockup_detector function should be called before secondary booting */
	dbg_snapshot_allcorelockup_detector_init();
}

static int __init dbg_snapshot_utils_save_systems_all(void)
{
	if (!dbg_snapshot_get_enable())
		return 0;

	smp_call_function(dbg_snapshot_save_system, NULL, 1);
	dbg_snapshot_save_system(NULL);

	return 0;
}
postcore_initcall(dbg_snapshot_utils_save_systems_all);
