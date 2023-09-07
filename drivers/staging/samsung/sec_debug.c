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
#include <linux/ptrace.h>
#include <linux/reboot.h>
#include <linux/sec_debug.h>
#include <linux/tick.h>
#include <linux/uaccess.h>
#ifdef CONFIG_PROC_AVC
#include <linux/proc_avc.h>
#endif

#define LK_RSV_ADDR		0x46000000
#define LK_RSV_SIZE		0x600000
#define SEC_EXTRA_INFO_BASE	0x43D00000
#define MAX_VER_LENGTH		64
static u32 __initdata sec_extra_info_base = SEC_EXTRA_INFO_BASE;
static u32 __initdata sec_extra_info_size = (SZ_1M>>1);

char sec_debug_bl_ver[MAX_VER_LENGTH];

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

int sec_save_context(int cpu, void *v_regs)
{
	unsigned long flags;
	struct pt_regs *regs = (struct pt_regs *)v_regs;
	struct pt_regs tmp;

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

	crash_setup_regs(&tmp, regs);
	crash_save_vmcoreinfo();
	if (cpu == _THIS_CPU) {
		crash_save_cpu(&tmp, smp_processor_id());
		__this_cpu_inc(coreregs_stored);
		pr_emerg("context saved(CPU:%d)\n", smp_processor_id());
	} else {
		crash_save_cpu(&tmp, cpu);
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

static int __init sec_debug_check_blversion(char *str)
{
	strlcpy(sec_debug_bl_ver, str, MAX_VER_LENGTH);

	return 0;
}
early_param("androidboot.bootloader", sec_debug_check_blversion);

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
	else if (!strncmp(buf, "Crash Key", 9))
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "User Crash Key", 14))
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_USER_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strncmp(buf, "HSIC Disconnected", 17))
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_HSIC_DISCONNECTED);
	else
		LAST_RR_SET(upload_reason, UPLOAD_CAUSE_KERNEL_PANIC);
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

	if (tsk->state == TASK_RUNNING
			|| tsk->state == TASK_UNINTERRUPTIBLE
			|| tsk->mm == NULL) {
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
	pr_info("     pid      uTime    sTime      exec(ns)  stat  cpu       wchan           user_pc        task_struct       comm   sym_wchan\n");
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

static struct sec_debug_shared_info *sec_debug_info;

static void sec_debug_init_base_buffer(void)
{
	sec_debug_info = (struct sec_debug_shared_info *)phys_to_virt(sec_extra_info_base);

	if (sec_debug_info) {
		sec_debug_info->magic[0] = SEC_DEBUG_SHARED_MAGIC0;
		sec_debug_info->magic[1] = SEC_DEBUG_SHARED_MAGIC1;
		sec_debug_info->magic[2] = SEC_DEBUG_SHARED_MAGIC2;
		sec_debug_info->magic[3] = SEC_DEBUG_SHARED_MAGIC3;

		//sec_debug_set_kallsyms_info(sec_debug_info);
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
		sec_debug_init_extra_info(sec_debug_info, sec_extra_info_size);
#endif
	}

	pr_info("%s, base(virt):0x%lx size:0x%x\n", __func__, (unsigned long)sec_debug_info, sec_extra_info_size);
}

static void sec_free_dbg_mem(phys_addr_t base, phys_addr_t size)
{
	memblock_free_late(base, size);
	pr_info("%s: freed memory [%#016llx-%#016llx]\n",
		     __func__, (u64)base, (u64)base + size - 1);
}

static int __init sec_debug_init(void)
{
	/* In order to skip DRAM Calibration after Soft-Reset*/
	wd_dram_reserved_mode(true);
		
	sec_debug_init_base_buffer();

	if (SEC_DEBUG_LEVEL(kernel) == 0) {
		sec_free_dbg_mem(LK_RSV_ADDR, LK_RSV_SIZE);
	}
#ifdef CONFIG_PROC_AVC
	sec_avc_log_init();
#endif
	return 0;
}
postcore_initcall(sec_debug_init);

#ifdef CONFIG_USER_RESET_DEBUG
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

#ifdef CONFIG_SEC_DUMP_SUMMARY
static ssize_t sec_reset_summary_info_proc_read(struct file *file, char __user *buf,
		size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if(reset_reason < RR_D || reset_reason > RR_P)
		return -ENOENT;

	if (pos >= last_summary_size)
		return -ENOENT;

	count = min(len, (size_t)(last_summary_size - pos));
	if (copy_to_user(buf, last_summary_buffer + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations sec_reset_summary_info_proc_fops = {
	.owner = THIS_MODULE,
	.read = sec_reset_summary_info_proc_read,
};
#endif

static int __init sec_debug_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason", S_IWUGO, NULL,
		&sec_reset_reason_proc_fops);

	if (!entry)
		return -ENOMEM;

	entry = proc_create("store_lastkmsg", S_IWUGO, NULL,
		&sec_debug_reset_reason_store_lastkmsg_proc_fops);

	if (!entry)
		return -ENOMEM;

#ifdef CONFIG_SEC_DUMP_SUMMARY
	entry = proc_create("reset_summary", S_IWUGO, NULL,
		&sec_reset_summary_info_proc_fops);

	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, last_summary_size);
#endif
	return 0;
}
device_initcall(sec_debug_reset_reason_init);
#endif /* CONFIG_USER_RESET_DEBUG */

#ifdef CONFIG_SEC_FILE_LEAK_DEBUG
int sec_debug_print_file_list(void)
{
	int i = 0;
	unsigned int nCnt = 0;
	struct file *file = NULL;
	struct files_struct *files = current->files;
	const char *pRootName = NULL;
	const char *pFileName = NULL;
	int ret=0;

	nCnt=files->fdt->max_fds;

	pr_err("[Opened file list of process %s(PID:%d, TGID:%d) :: %d]\n",
		current->group_leader->comm, current->pid, current->tgid, nCnt);

	for (i = 0; i < nCnt; i++) {
		rcu_read_lock();
		file = fcheck_files(files, i);

		pRootName = NULL;
		pFileName = NULL;

		if (file) {
			if (file->f_path.mnt
				&& file->f_path.mnt->mnt_root
				&& file->f_path.mnt->mnt_root->d_name.name)
				pRootName = file->f_path.mnt->mnt_root->d_name.name;

			if (file->f_path.dentry && file->f_path.dentry->d_name.name)
				pFileName = file->f_path.dentry->d_name.name;

			pr_err("[%04d]%s%s\n",i,
				pRootName == NULL ? "null" : pRootName,
				pFileName == NULL ? "null" : pFileName);
			ret++;
		}
		rcu_read_unlock();
	}
	if(ret > nCnt - 50)
		return 1;
	else
		return 0;
}

void sec_debug_EMFILE_error_proc(unsigned long files_addr)
{
	if (files_addr != (unsigned long)(current->files)) {
		pr_error("Too many open files Error at %pS\n"
			"%s(%d) thread of %s process tried fd allocation by proxy.\n"
			"files_addr = 0x%lx, current->files=0x%p\n",
			__builtin_return_address(0),
			current->comm, current->tgid, current->group_leader->comm,
			files_addr, current->files);
		return;
	}

	pr_err("Too many open files(%d:%s) at %pS\n",
		current->tgid, current->group_leader->comm,
		__builtin_return_address(0));

	if (!SEC_DEBUG_LEVEL(kernel))
		return;

	/* We check EMFILE error in only "system_server",
	   "mediaserver" and "surfaceflinger" process.*/
	if (!strcmp(current->group_leader->comm, "system_server")
		|| !strcmp(current->group_leader->comm, "mediaserver")
		|| !strcmp(current->group_leader->comm, "surfaceflinger")){
		if (sec_debug_print_file_list() == 1) {
			panic("Too many open files");
		}
	}
}
#endif

#ifdef CONFIG_SEC_DUMP_SUMMARY
struct sec_debug_summary *summary_info;
static char *sec_summary_log_buf;
static unsigned long sec_summary_log_size;
static unsigned long reserved_out_buf;
static unsigned long reserved_out_size;
static char *last_summary_buffer;
static size_t last_summary_size;

void sec_debug_summary_set_reserved_out_buf(unsigned long buf, unsigned long size)
{
	reserved_out_buf = buf;
	reserved_out_size = size;
}

static int __init sec_summary_log_setup(char *str)
{
	unsigned long size = memparse(str, &str);
	unsigned long base = 0;

	if (dynsyslog_on == 0)
		return 0;

	/* If we encounter any problem parsing str ... */
	if (!size || *str != '@' || kstrtoul(str + 1, 0, &base)) {
		pr_err("%s: failed to parse address.\n", __func__);
		goto out;
	}

	last_summary_size = size;

	// dump_summary size set 1MB in low level.
	if (!SEC_DEBUG_LEVEL(kernel))
		size = 0x100000;

	if (memblock_is_region_reserved(base, size) ||
			memblock_reserve(base, size)) {
		pr_err("%s: failed to reserve size:0x%lx " \
				"at base 0x%lx\n", __func__, size, base);
		goto out;
	}
	pr_info("%s, base:0x%lx size:0x%lx\n", __func__, base, size);

	sec_summary_log_buf = phys_to_virt(base);
	sec_summary_log_size = round_up(sizeof(struct sec_debug_summary), PAGE_SIZE);
	last_summary_buffer = phys_to_virt(base + sec_summary_log_size);
	sec_debug_summary_set_reserved_out_buf(base + sec_summary_log_size, (size - sec_summary_log_size));
out:
	return 0;
}
__setup("sec_summary_log=", sec_summary_log_setup);

int sec_debug_summary_init(void)
{
	int offset = 0;

	if(!sec_summary_log_buf) {
		pr_info("no summary buffer\n");
		return 0;
	}

	summary_info = (struct sec_debug_summary *)sec_summary_log_buf;
	memset(summary_info, 0, sizeof(struct sec_debug_summary));

	exynos_ss_summary_set_sched_log_buf(summary_info);

	sec_debug_summary_set_logger_info(&summary_info->kernel.logger_log);
	offset += sizeof(struct sec_debug_summary);

	summary_info->kernel.cpu_info.cpu_active_mask_paddr = virt_to_phys(cpu_active_mask);
	summary_info->kernel.cpu_info.cpu_online_mask_paddr = virt_to_phys(cpu_online_mask);
	offset += sec_debug_set_cpu_info(summary_info,sec_summary_log_buf+offset);

	summary_info->kernel.nr_cpus = CONFIG_NR_CPUS;
	summary_info->reserved_out_buf = reserved_out_buf;
	summary_info->reserved_out_size = reserved_out_size;
	summary_info->magic[0] = SEC_DEBUG_SUMMARY_MAGIC0;
	summary_info->magic[1] = SEC_DEBUG_SUMMARY_MAGIC1;
	summary_info->magic[2] = SEC_DEBUG_SUMMARY_MAGIC2;
	summary_info->magic[3] = SEC_DEBUG_SUMMARY_MAGIC3;

	sec_debug_summary_set_kallsyms_info(summary_info);

	pr_debug("%s, sec_debug_summary_init done [%d]\n", __func__, offset);

	return 0;
}
late_initcall(sec_debug_summary_init);

int sec_debug_save_panic_info(const char *str, unsigned long caller)
{
	if(!sec_summary_log_buf || !summary_info)
		return 0;
	snprintf(summary_info->kernel.excp.panic_caller,
		sizeof(summary_info->kernel.excp.panic_caller), "%pS", (void *)caller);
	snprintf(summary_info->kernel.excp.panic_msg,
		sizeof(summary_info->kernel.excp.panic_msg), "%s", str);
	snprintf(summary_info->kernel.excp.thread,
		sizeof(summary_info->kernel.excp.thread), "%s:%d", current->comm,
		task_pid_nr(current));

	return 0;
}
#endif /* CONFIG_SEC_DUMP_SUMMARY */
