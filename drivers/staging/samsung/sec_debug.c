/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
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
#include <linux/kmsg_dump.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/of.h>
#include <linux/proc_fs.h>
#include <linux/ptrace.h>
#include <linux/reboot.h>
#include <linux/sec_debug.h>
#include <linux/tick.h>
#include <linux/uaccess.h>
#ifdef CONFIG_SEC_DEBUG_GPIO
#include <linux/platform_device.h>
#endif
#ifdef CONFIG_PROC_AVC
#include <linux/proc_avc.h>
#endif

#define	PL_RSV_ADDR		0x43d00000
#define	PL_RSV_SIZE		0x200000
#define	LK_RSV_ADDR		0x41e00000
#define	LK_RSV_SIZE		0x600000
#define	MINIR_RSV_ADDR		0x43ff0000
#define	MINIR_RSV_SIZE		0x10000
/* layout
	   0: magic (4B)
      4~1023: panic string (1020B)
 0x400~0x7ff: panic Extra Info (1KB)
0x800~0x1000: panic dumper log
      0x4000: copy of magic
 */
#define SEC_DEBUG_MAGIC_PA	0x43030000
#define SEC_DEBUG_MAGIC_VA	phys_to_virt(SEC_DEBUG_MAGIC_PA)
#define SEC_DEBUG_EXTRA_INFO_VA	(SEC_DEBUG_MAGIC_VA + 0x400)

/* layout of extraInfo for bigdata
	"KTIME":""
	"FAULT":""
	"BUG":""
	"PANIC":
	"PC":""
	"LR":""
	"STACK":""
 */
struct sec_debug_panic_extra_info {
	unsigned long fault_addr;
	char bug_buf[SZ_64];
	char panic_buf[SZ_64];
	unsigned long pc;
	unsigned long lr;
	char backtrace[SZ_512];
};

static struct sec_debug_panic_extra_info panic_extra_info;
DEFINE_PER_CPU(struct sec_debug_core_t, sec_debug_core_reg);
DEFINE_PER_CPU(struct sec_debug_mmu_reg_t, sec_debug_mmu_reg);
union sec_debug_level_t sec_debug_level = { .en.kernel_fault = 1, };
module_param_named(enable, sec_debug_level.en.kernel_fault, ushort, 0644);
module_param_named(enable_user, sec_debug_level.en.user_fault, ushort, 0644);
module_param_named(level, sec_debug_level.uint_val, uint, 0644);

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
	if (sec_debug_level.en.kernel_fault == 1
	    && sec_debug_level.en.user_fault == 1)
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

enum {
	LK_VOLUME_UP,
	LK_VOLUME_DOWN,
	LK_POWER,
	LK_HOMEPAGE,
	LK_MAX,
} local_key_map;

static int key_map(unsigned int code)
{
	switch (code) {
	case KEY_VOLUMEDOWN:
		return LK_VOLUME_DOWN;
	case KEY_VOLUMEUP:
		return LK_VOLUME_UP;
	case KEY_POWER:
		return LK_POWER;
	case KEY_HOMEPAGE:
		return LK_HOMEPAGE;
	default:
		return LK_MAX;
	}
}

/* Input sequence 9530 */
#define	CRASH_COUNT_VOLUME_UP	9
#define CRASH_COUNT_VOLUME_DOWN	5
#define CRASH_COUNT_POWER	3

void _check_crash_user(unsigned int code, int onoff)
{
	static bool home_p;
	static int check_count;
	static int check_step;
	static bool local_key_state[LK_MAX];
	int key_index = key_map(code);

	if (key_index >= LK_MAX)
		return;

	if (onoff) {
		/* Check duplicate input */
		if (local_key_state[key_index])
			return;
		local_key_state[key_index] = true;

		if (code == KEY_HOMEPAGE) {
			check_step = 1;
			home_p = true;
			return;
		}
		if (home_p) {
			switch (check_step) {
			case 1:
				if (code == KEY_VOLUMEUP)
					check_count++;
				else {
					check_count = 0;
					check_step = 0;
					pr_info("Rest crach key check [%d]\n",
							__LINE__);
				}
				if (check_count == CRASH_COUNT_VOLUME_UP)
					check_step++;
				break;
			case 2:
				if (code == KEY_VOLUMEDOWN)
					check_count++;
				else {
					check_count = 0;
					check_step = 0;
					pr_info("Rest crach key check [%d]\n",
							__LINE__);
				}
				if (check_count == CRASH_COUNT_VOLUME_UP
						+ CRASH_COUNT_VOLUME_DOWN)
					check_step++;
				break;
			case 3:
				if (code == KEY_POWER)
					check_count++;
				else {
					check_count = 0;
					check_step = 0;
					pr_info("Rest crach key check [%d]\n",
							__LINE__);
				}
				if (check_count == CRASH_COUNT_VOLUME_UP
						+ CRASH_COUNT_VOLUME_DOWN
						+ CRASH_COUNT_POWER)
					panic("User Crash Key");
				break;
			default:
				break;
			}
		}
	} else {
		local_key_state[key_index] = false;
		if (code == KEY_HOMEPAGE) {
			check_count = 0;
			check_step = 0;
			home_p = false;
		}
	}
}

void sec_debug_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	if (!sec_debug_level.en.kernel_fault) {
		_check_crash_user(code, value);
		return;
	}

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
				if (loopcount == 2) {
					panic("Crash Key");
				}
			}
		}
	} else {
		if (code == KEY_VOLUMEUP) {
			volup_p = false;
		}
		if (code == KEY_VOLUMEDOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}

static void sec_debug_init_panic_extra_info(void)
{
	panic_extra_info.fault_addr = -1;
	panic_extra_info.pc = -1;
	panic_extra_info.lr = -1;
	strncpy(panic_extra_info.bug_buf, "N/A", 3);
	strncpy(panic_extra_info.panic_buf, "N/A", 3);
	strncpy(panic_extra_info.backtrace, "N/A\"", 3);
}

static void sec_debug_store_panic_extra_info(char* str)
{
	/* store panic extra info
		"KTIME":""	: kernel time
		"FAULT":""	: fault addr
		"BUG":""	: bug msg
		"PANIC":""	: panic buffer msg
		"PC":""		: pc val
		"LR":""		: link register val
		"STACK":""	: backtrace
	 */
	int panic_string_offset = 0;
	int cp_len;
	unsigned long rem_nsec;
	u64 ts_nsec = local_clock();

	rem_nsec = do_div(ts_nsec, 1000000000);

	cp_len = strlen(str);
	if (str[cp_len-1] == '\n')
		str[cp_len-1] = '\0';

	memset((void*)SEC_DEBUG_EXTRA_INFO_VA, 0, SZ_1K);

	panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA,
		"\"KTIME\":\"%lu.%06lu\",\n", (unsigned long)ts_nsec, rem_nsec / 1000);
	if (panic_extra_info.fault_addr != -1)
		panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
			"\"FAULT\":\"0x%lx\",\n", panic_extra_info.fault_addr);
	else
		panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
			"\"FAULT\":\"\",\n");

	if (strncmp(panic_extra_info.bug_buf, "N/A", 3))
		panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
			"\"BUG\":\"%s\",\n", panic_extra_info.bug_buf);
	else
		panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
			"\"BUG\":\"\",\n");

	panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
		"\"PANIC\":\"%s\",\n", str);

	if (panic_extra_info.pc != -1)
		panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
			"\"PC\":\"%pS\",\n", (void*)panic_extra_info.pc);
	else
		panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
			"\",\"PC\":\"\",\n");

	if (panic_extra_info.lr != -1)
		panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
			"\"LR\":\"%pS\",\n", (void*)panic_extra_info.lr);
	else
		panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
			"\"LR\":\"\",\n");

	cp_len = strlen(panic_extra_info.backtrace);
	if (panic_string_offset + cp_len > SZ_1K - 10)
		cp_len = SZ_1K - panic_string_offset - 10;

	if (cp_len > 0) {
		panic_string_offset += sprintf((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset,
		"\"STACK\":\"");
		strncpy((char *)SEC_DEBUG_EXTRA_INFO_VA + panic_string_offset, panic_extra_info.backtrace, cp_len);
	}
}

void sec_debug_store_fault_addr(unsigned long addr, struct pt_regs *regs)
{
	printk(KERN_ERR "%s:0x%lx\n", __func__, addr);

	panic_extra_info.fault_addr = addr;
	if (regs) {
		panic_extra_info.pc = regs->ARM_pc;
		panic_extra_info.lr = regs->ARM_lr;
	}
}

void sec_debug_store_bug_string(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vsnprintf(panic_extra_info.bug_buf, sizeof(panic_extra_info.bug_buf), fmt, args);
	va_end(args);
}

void sec_debug_store_backtrace(struct pt_regs *regs)
{
	char buf[64];
	struct stackframe frame;
	int offset = 0;
	int sym_name_len;
	register unsigned long sp asm ("sp");

	printk(KERN_ERR "%s\n", __func__);

	if (regs) {
		frame.fp = regs->ARM_fp;
		frame.sp = regs->ARM_sp;
		frame.pc = regs->ARM_pc;
	} else {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = sp;
		frame.pc = (unsigned long)sec_debug_store_backtrace;
	}

	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(&frame);
		if (ret < 0)
			break;

		snprintf(buf, sizeof(buf), "%pf", (void *)where);
		sym_name_len = strlen(buf);

		if (offset + sym_name_len > SZ_1K)
			break;

		if (offset)
			offset += sprintf((char*)panic_extra_info.backtrace+offset, " : ");

		sprintf((char*)panic_extra_info.backtrace+offset, "%s", buf);
		offset += sym_name_len;
	}
	sprintf((char*)panic_extra_info.backtrace+offset, "\"\n");
}

void sec_debug_set_upload_magic(unsigned magic, char *str)
{
	pr_emerg("sec_debug: set magic code (0x%x)\n", magic);

	*(unsigned int *)SEC_DEBUG_MAGIC_VA = magic;
	*(unsigned int *)(SEC_DEBUG_MAGIC_VA + SZ_4K - 4) = magic;

	if (str) {
		strncpy((char *)SEC_DEBUG_MAGIC_VA + 4, str, SZ_1K - 4);
		sec_debug_store_panic_extra_info(str);
	}
}

void sec_upload_cause(void *buf)
{
	SEC_LAST_RR_SET(is_upload, UPLOAD_MAGIC_UPLOAD);
	sec_debug_set_upload_magic(UPLOAD_MAGIC_UPLOAD, buf);

	if (!strncmp(buf, "User Fault", 10))
		SEC_LAST_RR_SET(upload_reason, UPLOAD_CAUSE_USER_FAULT);
	else if (!strncmp(buf, "Crash Key", 9))
		SEC_LAST_RR_SET(upload_reason, UPLOAD_CAUSE_FORCED_UPLOAD);
	else if (!strncmp(buf, "User Crash Key", 14))
		SEC_LAST_RR_SET(upload_reason, UPLOAD_CAUSE_USER_FORCED_UPLOAD);
	else if (!strncmp(buf, "CP Crash", 8))
		SEC_LAST_RR_SET(upload_reason, UPLOAD_CAUSE_CP_ERROR_FATAL);
	else if (!strncmp(buf, "HSIC Disconnected", 17))
		SEC_LAST_RR_SET(upload_reason, UPLOAD_CAUSE_HSIC_DISCONNECTED);
	else
		SEC_LAST_RR_SET(upload_reason, UPLOAD_CAUSE_KERNEL_PANIC);
}

static void sec_dump_one_task_info(struct task_struct *tsk, bool is_main)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'Z', 'X', 'x', 'K', 'W'};
	unsigned char idx = 0;
	unsigned int state = (tsk->state & TASK_REPORT) | tsk->exit_state;
	unsigned long wchan;
	unsigned long pc = 0;
	char symname[KSYM_NAME_LEN];
	int permitted;
	struct mm_struct *mm;

	permitted = ptrace_may_access(tsk, PTRACE_MODE_READ);
	mm = get_task_mm(tsk);
	if (mm) {
		if (permitted)
			pc = KSTK_EIP(tsk);
	}

	wchan = get_wchan(tsk);
	if (lookup_symbol_name(wchan, symname) < 0) {
		if (!ptrace_may_access(tsk, PTRACE_MODE_READ))
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

void free_dbg_mem(phys_addr_t base, phys_addr_t size)
{
	memblock_free_late(base, size);
	pr_info("freed memory : %uKB at %#.8x\n", size / SZ_1K, base);
}

static int __init sec_debug_init(void)
{
#ifdef CONFIG_OF
	struct device_node *root;
	const __be32 *chip_id, *hw_rev;

	root = of_find_node_by_path("/");
	if (root) {
		chip_id = of_get_property(root, "model_info-chip", NULL);
		hw_rev = of_get_property(root, "model_info-hw_rev", NULL);
		if (chip_id && hw_rev)
			pr_err("dtb model_info-chip : %d, hw_rev : %d\n",
				be32_to_cpu(*chip_id), be32_to_cpu(*hw_rev));
	}
#endif
#ifdef CONFIG_PROC_AVC
	sec_avc_log_init();
#endif
	/* Initialize flags */
	SEC_LAST_RR_SET(is_upload, UPLOAD_MAGIC_UPLOAD);
	SEC_LAST_RR_SET(upload_reason, UPLOAD_CAUSE_INIT);
	SEC_LAST_RR_SET(is_power_reset, SEC_POWER_OFF);
	SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_INIT);
	sec_debug_set_upload_magic(UPLOAD_MAGIC_UPLOAD, NULL);
	sec_debug_init_panic_extra_info();
	/* In order to skip DRAM Calibration after Soft-Reset*/
	wd_dram_reserved_mode(true);

	if (sec_debug_level.en.kernel_fault == 0) {
		free_dbg_mem(PL_RSV_ADDR, PL_RSV_SIZE);
		free_dbg_mem(LK_RSV_ADDR, LK_RSV_SIZE);
		free_dbg_mem(MINIR_RSV_ADDR, MINIR_RSV_SIZE);
	}
	return 0;
}
fs_initcall(sec_debug_init);

#ifdef CONFIG_USER_RESET_DEBUG
enum sec_debug_reset_reason_t {
	RR_S = 1,
	RR_W = 2,
	RR_D = 3,
	RR_K = 4,
	RR_M = 5,
	RR_P = 6,
	RR_R = 7,
	RR_B = 8,
	RR_N = 9,
	RR_T = 10,
};

static unsigned reset_reason = RR_N;
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

static int set_reset_extra_info_proc_show(struct seq_file *m, void *v)
{
	char buf[SZ_1K];

	memcpy(buf, (char *)SEC_DEBUG_EXTRA_INFO_VA, SZ_1K);

	if (reset_reason == RR_K)
		seq_printf(m,buf);
	else
		return -ENOENT;

	return 0;
}

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

static int
sec_reset_reason_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_reset_reason_proc_show, NULL);
}

static const struct file_operations sec_reset_reason_proc_fops = {
	.open = sec_reset_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int
sec_reset_extra_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_reset_extra_info_proc_show, NULL);
}

static const struct file_operations sec_reset_extra_info_proc_fops = {
	.open = sec_reset_extra_info_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init sec_debug_reset_reason_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason", S_IWUGO, NULL,
		&sec_reset_reason_proc_fops);

	if (!entry)
		return -ENOMEM;

	entry = proc_create("reset_reason_extra_info", S_IRUSR, NULL,
		&sec_reset_extra_info_proc_fops);

	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, SZ_1K);
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
void sec_debug_print_file_list(void)
{
	int i = 0;
	unsigned int nCnt = 0;
	struct file *file = NULL;
	struct files_struct *files = current->files;
	const char *pRootName = NULL;
	const char *pFileName = NULL;

	nCnt = files->fdt->max_fds;

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

			pr_err("[%04d]%s%s\n", i,
			       pRootName == NULL ? "null" : pRootName,
			       pFileName == NULL ? "null" : pFileName);
		}
		rcu_read_unlock();
	}
}

void sec_debug_EMFILE_error_proc(unsigned long files_addr)
{
	if (files_addr != (unsigned long)(current->files)) {
		pr_err("Too many open files Error at %pS\n"
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

	if (!sec_debug_level.en.kernel_fault)
		return;

	/* We check EMFILE error in only "system_server",
	   "mediaserver" and "surfaceflinger" process. */
	if (!strcmp(current->group_leader->comm, "system_server")
		|| !strcmp(current->group_leader->comm, "mediaserver")
		|| !strcmp(current->group_leader->comm, "surfaceflinger")) {
		sec_debug_print_file_list();
		panic("Too many open files");
	}
}
#endif

#ifdef CONFIG_SEC_PARAM
#define SEC_PARAM_NAME "/dev/block/param"
struct sec_param_data_s {
	struct work_struct sec_param_work;
	unsigned long offset;
	char val;
};

static struct sec_param_data_s sec_param_data;
static DEFINE_MUTEX(sec_param_mutex);

static void sec_param_update(struct work_struct *work)
{
	int ret = -1;
	struct file *fp;
	struct sec_param_data_s *param_data =
		container_of(work, struct sec_param_data_s, sec_param_work);

	fp = filp_open(SEC_PARAM_NAME, O_WRONLY | O_SYNC, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));
		return;
	}
	pr_info("%s: set param %c at %lu\n", __func__,
				param_data->val, param_data->offset);
	ret = fp->f_op->llseek(fp, param_data->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}

	ret = fp->f_op->write(fp, &param_data->val, 1, &(fp->f_pos));
	if (ret < 0)
		pr_err("%s: write error! %d\n", __func__, ret);

close_fp_out:
	if (fp)
		filp_close(fp, NULL);

	pr_info("%s: exit %d\n", __func__, ret);
}

/*
  success : ret >= 0
  fail : ret < 0
 */
int sec_set_param(unsigned long offset, char val)
{
	int ret = -1;

	mutex_lock(&sec_param_mutex);

	if ((offset < CM_OFFSET) || (offset > CM_OFFSET + CM_OFFSET_LIMIT))
		goto unlock_out;

	switch (val) {
	case PARAM_OFF:
	case PARAM_ON:
		goto set_param;
	default:
		if (val >= PARAM_TEST0 && val < PARAM_MAX)
			goto set_param;
		goto unlock_out;
	}

set_param:
	sec_param_data.offset = offset;
	sec_param_data.val = val;

	schedule_work(&sec_param_data.sec_param_work);

	/* how to determine to return success or fail ? */

	ret = 0;
unlock_out:
	mutex_unlock(&sec_param_mutex);
	return ret;
}

static int __init sec_param_work_init(void)
{
	pr_info("%s: start\n", __func__);

	sec_param_data.offset = 0;
	sec_param_data.val = '0';

	INIT_WORK(&sec_param_data.sec_param_work, sec_param_update);

	return 0;
}

static void __exit sec_param_work_exit(void)
{
	cancel_work_sync(&sec_param_data.sec_param_work);
	pr_info("%s: exit\n", __func__);
}
module_init(sec_param_work_init);
module_exit(sec_param_work_exit);
#endif /* CONFIG_SEC_PARAM */

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
	if (!sec_debug_level.en.kernel_fault)
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

#ifdef CONFIG_SEC_DEBUG_GPIO
static int sec_debug_gpio_probe(struct platform_device *pdev)
{

	struct pinctrl *pctrl;

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		pctrl = devm_pinctrl_get_select_default(&pdev->dev);
		if (IS_ERR(pctrl)) {
			dev_err(&pdev->dev, "Failed to select pinctrl of sec_debug_gpio\n");
			return -EINVAL;
		}
	}
#endif
	return 0;
}

static int sec_debug_gpio_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id secdebug_gpio_of_ids[] = {
	{.compatible = "samsung,debug-gpio",},
	{}
};
#endif

static struct platform_driver gpio_driver = {
	.probe = sec_debug_gpio_probe,
	.remove = sec_debug_gpio_remove,
	.driver = {
		   .name = "sec_debug-gpio",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(secdebug_gpio_of_ids),
#endif
		   },
};

static int __init sec_debug_gpio_init(void)
{
	return platform_driver_register(&gpio_driver);
}

static void __exit sec_debug_gpio_exit(void)
{
	platform_driver_unregister(&gpio_driver);
}

device_initcall(sec_debug_gpio_init);
module_exit(sec_debug_gpio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SAMSUNG Electronics");
MODULE_DESCRIPTION("SAMSUNG DEBUG DRIVER");
#endif /* CONFIG_SEC_DEBUG_GPIO */
