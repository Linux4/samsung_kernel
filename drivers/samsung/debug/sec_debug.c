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
#include <linux/bootmem.h>
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
#include <linux/sec_debug.h>
#include <linux/tick.h>
#include <linux/uaccess.h>
#include <linux/sched/mm.h>
#include <linux/sched/debug.h>
#include <linux/nmi.h>
#include <linux/sec_hard_reset_hook.h>

static u32 __initdata sec_extra_info_base = SEC_EXTRA_INFO_BASE;
static u32 __initdata sec_extra_info_size = SZ_64K;

DEFINE_PER_CPU(unsigned char, coreregs_stored);
DEFINE_PER_CPU(struct pt_regs, sec_aarch64_core_reg);
DEFINE_PER_CPU(sec_debug_mmu_reg_t, sec_aarch64_mmu_reg);

union sec_debug_level_t sec_debug_level = { .en.kernel_fault = 1, };
module_param_named(enable, sec_debug_level.en.kernel_fault, ushort, 0644);
module_param_named(enable_user, sec_debug_level.en.user_fault, ushort, 0644);
module_param_named(level, sec_debug_level.uint_val, uint, 0644);

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

	__inner_flush_dcache_all();

	local_irq_restore(flags);

	return 0;
}

static int __init sec_debug_levelset(char *str)
{
	int level;

	if (get_option(&str, &level)) {
		memcpy(&sec_debug_level, &level, sizeof(sec_debug_level));
		printk(KERN_INFO "sec_debug_level:0x%x\n", sec_debug_level.uint_val);
		return 0;
	}

	return -EINVAL;
}
early_param("sec_debug", sec_debug_levelset);

int sec_debug_get_debug_level(void)
{
	return sec_debug_level.uint_val;
}

static void sec_debug_user_fault_dump(void)
{
	if (SEC_DEBUG_LEVEL(kernel) == 1
	    && SEC_DEBUG_LEVEL(user) == 1)
		panic("User Fault");
}

static ssize_t
sec_debug_user_fault_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *offs)
{
	char buf[100];

	if (count > sizeof(buf) - 1)
		return -EINVAL;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[count] = '\0';

	if (strncmp(buf, "dump_user_fault", 15) == 0)
		sec_debug_user_fault_dump();

	return count;
}

static const struct file_operations sec_debug_user_fault_proc_fops = {
	.write = sec_debug_user_fault_write,
};

static int __init sec_debug_user_fault_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("user_fault", S_IWUSR | S_IWGRP, NULL,
			    &sec_debug_user_fault_proc_fops);
	if (!entry)
		return -ENOMEM;
	return 0;
}
device_initcall(sec_debug_user_fault_init);

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
struct tsp_dump_callbacks dump_callbacks;
static inline void tsp_dump(void)
{
	pr_err("[TSP] dump_tsp_log : %d\n", __LINE__);
	if (dump_callbacks.inform_dump)
		dump_callbacks.inform_dump();
}
#endif

void sec_debug_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;
#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
	static int tsp_dump_count;
#endif

	if (!SEC_DEBUG_LEVEL(kernel))
		return;

	pr_debug("%s: code is %d, value is %d\n", __func__, code, value);

	/* Enter Force Upload
	 *  Hold volume down key first
	 *  and then press power key twice
	 *  and volume up key should not be pressed
	 */
	if (value) {
		if (code == KEY_VOLUMEUP)
			volup_p = true;
		if (code == KEY_VOLUMEDOWN)
			voldown_p = true;
		if (!volup_p && voldown_p) {
			if (code == KEY_POWER) {
				pr_info
				    ("%s: count for enter forced upload : %d\n",
				     __func__, ++loopcount);
				if (loopcount == 2)
					panic("Crash Key");
			}
		}

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
		/* dump TSP rawdata
		 *	Hold volume up key first
		 *	and then press home key twice
		 *	and volume down key should not be pressed
		 */
		if (volup_p && !voldown_p) {
			if (code == KEY_HOMEPAGE) {
				pr_info("%s: count to dump tsp rawdata : %d\n",
					 __func__, ++tsp_dump_count);
				if (tsp_dump_count == 2) {
					tsp_dump();
					tsp_dump_count = 0;
				}
			}
		}
#endif
	} else {
		if (code == KEY_VOLUMEUP) {
			volup_p = false;
#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
			tsp_dump_count = 0;
#endif
		}
		if (code == KEY_VOLUMEDOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
static void sec_debug_set_extra_info_upload(char *str)
{
	if (str) {
		sec_debug_set_extra_info_panic(str);
		sec_debug_finish_extra_info();
	}
}
#endif

void sec_upload_cause(void *buf)
{
	LAST_RR_SET(is_upload, UPLOAD_MAGIC_UPLOAD);
	
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	sec_debug_set_extra_info_upload(buf);
#endif

	if (!strncmp(buf, "User Fault", 10))
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_USER_FAULT);
	else if (is_hard_reset_occurred())
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_FORCED_UPLOAD);		
	else if (!strncmp(buf, "Crash Key", 9))
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "User Crash Key", 14))
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_USER_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strncmp(buf, "Watchdog", 8))
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_WATCHDOG);
	else
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_KERNEL_PANIC);

	LAST_RR_MEMCPY(panic_str, buf, PANIC_STRBUF_LEN);		
}

static void sec_dump_one_task_info(struct task_struct *tsk, bool is_main)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'Z', 'X', 'x', 'K', 'W'};
	unsigned char idx = 0;
	unsigned long state = (tsk->state & TASK_REPORT) | tsk->exit_state;
	unsigned long wchan;
	unsigned long pc = 0;
	char symname[KSYM_NAME_LEN];
	int permitted;
	struct mm_struct *mm;

	permitted = ptrace_may_access(tsk, PTRACE_MODE_READ_FSCREDS | PTRACE_MODE_NOAUDIT);
	mm = get_task_mm(tsk);
	if (mm) {
		if (permitted)
			pc = KSTK_EIP(tsk);
	}

	wchan = get_wchan(tsk);
	if (lookup_symbol_name(wchan, symname) < 0) {
		if (!ptrace_may_access(tsk, PTRACE_MODE_READ_FSCREDS))
			snprintf(symname, KSYM_NAME_LEN,  "_____");
		else
			snprintf(symname, KSYM_NAME_LEN, "%lu", wchan);
	}

	while (state) {
		idx++;
		state >>= 1;
	}

	touch_softlockup_watchdog();

	pr_info("%8d %8d %8d %16lld %c(%d) %3d  %16lx %16lx  %16lx %c %16s [%s]\n",
			tsk->pid, (int)(tsk->utime), (int)(tsk->stime),
			tsk->se.exec_start, state_array[idx], (int)(tsk->state),
			task_cpu(tsk), wchan, pc, (unsigned long)tsk,
			is_main ? '*' : ' ', tsk->comm, symname);

	if (tsk->state == TASK_RUNNING || task_contributes_to_load(tsk)) {
			print_worker_info(KERN_INFO, tsk);
			show_stack(tsk, NULL);
			pr_info("\n");
	}
}

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next,
				struct task_struct,
				thread_group);
}

void sec_dump_task_info(void)
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

static int sec_debug_check_magic(struct sec_debug_shared_info *sdi)
{
	if (sdi->magic[0] != SEC_DEBUG_SHARED_MAGIC0) {
		pr_crit("%s: wrong magic 0: %x|%x\n",
				__func__, sdi->magic[0], SEC_DEBUG_SHARED_MAGIC0);
		return 0;
	}

	if (sdi->magic[1] != SEC_DEBUG_SHARED_MAGIC1) {
		pr_crit("%s: wrong magic 1: %x|%x\n",
				__func__, sdi->magic[1], SEC_DEBUG_SHARED_MAGIC1);
		return 0;
	}

	if (sdi->magic[2] != SEC_DEBUG_SHARED_MAGIC2) {
		pr_crit("%s: wrong magic 2: %x|%x\n",
				__func__, sdi->magic[2], SEC_DEBUG_SHARED_MAGIC2);
		return 0;
	}

	if (sdi->magic[3] != SEC_DEBUG_SHARED_MAGIC3) {
		pr_crit("%s: wrong magic 3: %x|%x\n",
				__func__, sdi->magic[3], SEC_DEBUG_SHARED_MAGIC3);
		return 0;
	}

	return 1;
}

static struct sec_debug_shared_info *sec_debug_info;

static void sec_debug_init_base_buffer(void)
{
	int magic_status = 0;

	sec_debug_info = (struct sec_debug_shared_info *)phys_to_virt(sec_extra_info_base);

	if (sec_debug_info) {
		if (sec_debug_check_magic(sec_debug_info))
			magic_status = 1;

		sec_debug_info->magic[0] = SEC_DEBUG_SHARED_MAGIC0;
		sec_debug_info->magic[1] = SEC_DEBUG_SHARED_MAGIC1;
		sec_debug_info->magic[2] = SEC_DEBUG_SHARED_MAGIC2;
		sec_debug_info->magic[3] = SEC_DEBUG_SHARED_MAGIC3;

		//sec_debug_set_kallsyms_info(sec_debug_info);
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
		sec_debug_init_extra_info(sec_debug_info, sec_extra_info_size, magic_status);
#endif
	}

	pr_info("%s, base(virt):0x%lx size:0x%x\n", __func__, (unsigned long)sec_debug_info, sec_extra_info_size);
}

#if 0
static void sec_free_dbg_mem(phys_addr_t base, phys_addr_t size)
{
	memblock_free_late(base, size);
	pr_info("%s: freed memory [%#016llx-%#016llx]\n",
		     __func__, (u64)base, (u64)base + size - 1);
}
#endif

static int __init sec_debug_init(void)
{
	sec_debug_init_base_buffer();

	smp_call_function(sec_debug_init_mmu, NULL, 1);
	sec_debug_init_mmu(NULL);

	return 0;
}
postcore_initcall(sec_debug_init);

unsigned int reset_reason = RR_N;
module_param_named(reset_reason, reset_reason, uint, 0644);

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

static const struct file_operations sec_reset_reason_proc_fops = {
	.open = sec_reset_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
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

static const struct file_operations sec_debug_reset_reason_store_lastkmsg_proc_fops = {
	.open = sec_debug_reset_reason_store_lastkmsg_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * proc/extra
 * RSTCNT (32) + PC (256) + LR (256) (MAX: 544)
 * + BUG (256) + PANIC (256) (MAX: 512)
 */
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
extern struct sec_debug_panic_extra_info *sec_debug_extra_info_backup;
static int sec_debug_reset_reason_extra_show(struct seq_file *m, void *v)
{
	ssize_t size = 0;
	char buf[SZ_1K];

	if (!sec_debug_extra_info_backup)
		return -ENOENT;
	
	size += scnprintf((char *)(buf + size), SZ_1K - size,
			"%s: %s ",
			sec_debug_extra_info_backup->item[INFO_RSTCNT].key,
			sec_debug_extra_info_backup->item[INFO_RSTCNT].val);

	size += scnprintf((char *)(buf + size), SZ_1K - size,
			"%s: %s ",
			sec_debug_extra_info_backup->item[INFO_PC].key,
			sec_debug_extra_info_backup->item[INFO_PC].val);

	size += scnprintf((char *)(buf + size), SZ_1K - size,
			"%s: %s ",
			sec_debug_extra_info_backup->item[INFO_LR].key,
			sec_debug_extra_info_backup->item[INFO_LR].val);

	size += scnprintf((char *)(buf + size), SZ_1K - size,
			"%s: %s ",
			sec_debug_extra_info_backup->item[INFO_BUG].key,
			sec_debug_extra_info_backup->item[INFO_BUG].val);

	size += scnprintf((char *)(buf + size), SZ_1K - size,
			"%s: %s ",
			sec_debug_extra_info_backup->item[INFO_PANIC].key,
			sec_debug_extra_info_backup->item[INFO_PANIC].val);

	seq_printf(m, buf);

	return 0;
}

static int sec_debug_reset_reason_extra_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_debug_reset_reason_extra_show, NULL);
}

static const struct file_operations sec_debug_reset_reason_extra_proc_fops = {
	.open = sec_debug_reset_reason_extra_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static int __init sec_debug_reset_reason_init(void)
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

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	entry = proc_create("extra", 0400, NULL, 
		&sec_debug_reset_reason_extra_proc_fops);

	if (!entry)
		return -ENOMEM;
#endif

	return 0;
}
device_initcall(sec_debug_reset_reason_init);
