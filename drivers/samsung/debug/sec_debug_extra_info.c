/*
 *sec_debug_extrainfo.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
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
#include <linux/sec_debug.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>
#include <asm/stacktrace.h>
#include <asm/esr.h>

#define SZ_960	0x000003c0
#define EXTRA_VERSION	"TE15"

struct sec_debug_panic_extra_info sec_debug_extra_info_init = {
	.item = {
		{"ID",		"", SZ_32},
		{"KTIME",	"", SZ_8},
		{"BIN",		"", SZ_16},
		{"FAULT",	"", SZ_32},
		{"BUG",		"", SZ_64},
		{"PANIC",	"", SZ_64},
		{"PC",		"", SZ_64},
		{"LR",		"", SZ_64},
		{"STACK",	"", SZ_256},
		{"RR",		"", SZ_8},
		{"RSTCNT", 	"", SZ_4},
		{"QBI",		"", SZ_16},
		{"DPM",		"", SZ_32},
		{"SMP",		"", SZ_8},
		{"ETC",		"", SZ_256},
		{"ESR",		"", SZ_64},
		{"PCB",		"", SZ_8},
		{"SMD",		"", SZ_16},
		{"CHI",		"", SZ_4},
		{"KLG",		"", SZ_256},
		{"LEV",		"", SZ_4},

		/* extrb reset information */
		{"ID",		"", SZ_32},

		/* extrc reset information */
		{"ID",		"", SZ_32},

		/* extrm reset information */
		{"ID",		"", SZ_32},
		{"RR",		"", SZ_8},
		{"MFC",		"", SZ_960},
	}
};

struct sec_debug_panic_extra_info *sec_debug_extra_info;
struct sec_debug_panic_extra_info *sec_debug_extra_info_backup;
char *sec_debug_extra_info_buf;
/******************************************************************************
 * sec_debug_init_extra_info() - function to init extra info
 *
 * This function simply initialize each filed of sec_debug_panic_extra_info.
******************************************************************************/

void sec_debug_init_extra_info(struct sec_debug_shared_info *sec_debug_info,
				 phys_addr_t sec_extra_info_size, int magic_status)
{
	if (sec_debug_info) {
		pr_crit("%s: magic: %d\n", __func__, magic_status);
		sec_debug_extra_info = &sec_debug_info->sec_debug_extra_info;

		if (magic_status) {
			if (reset_reason == RR_K || reset_reason == RR_D || reset_reason == RR_P) {
				sec_debug_extra_info_backup = &sec_debug_info->sec_debug_extra_info_backup;
				sec_debug_extra_info_buf = (char *)sec_debug_info + sec_extra_info_size - SZ_1K;
				memcpy(sec_debug_extra_info_backup, sec_debug_extra_info,
						sizeof(struct sec_debug_panic_extra_info));
			}
		}

		if (sec_debug_extra_info)
			memcpy(sec_debug_extra_info, &sec_debug_extra_info_init,
					sizeof(sec_debug_extra_info_init));
	}
}

/******************************************************************************
 * sec_debug_set_extra_info() - function to set each extra info field
 *
 * This function simply set each filed of sec_debug_panic_extra_info.
******************************************************************************/

void sec_debug_set_extra_info(enum sec_debug_extra_buf_type type,
				const char *fmt, ...)
{
	va_list args;

	if (sec_debug_extra_info) {
		if (!strlen(sec_debug_extra_info->item[type].val)) {
			va_start(args, fmt);
			vsnprintf(sec_debug_extra_info->item[type].val,
					sec_debug_extra_info->item[type].max, fmt, args);
			va_end(args);
		}
	}
}

/******************************************************************************
 * sec_debug_store_extra_info - function to export extra info
 *
 * This function finally export the extra info to destination buffer.
 * The contents of buffer will be deliverd to framework at the next booting.
*****************************************************************************/

void sec_debug_store_extra_info(int start, int end)
{
	int i;
	int maxlen = MAX_EXTRA_INFO_KEY_LEN + MAX_EXTRA_INFO_VAL_LEN + 10;
	char *ptr = sec_debug_extra_info_buf;

	/* initialize extra info output buffer */
	memset((void *)ptr, 0, SZ_1K);

	if (!sec_debug_extra_info_backup)
		return;

	ptr += snprintf(ptr, maxlen, "\"%s\":\"%s\"", sec_debug_extra_info_backup->item[start].key,
		sec_debug_extra_info_backup->item[start].val);
		

	for (i = start + 1; i < end; i++) {
		if (ptr + strnlen(sec_debug_extra_info_backup->item[i].key, MAX_EXTRA_INFO_KEY_LEN)
			 + strnlen(sec_debug_extra_info_backup->item[i].val, MAX_EXTRA_INFO_VAL_LEN)
			 + MAX_EXTRA_INFO_HDR_LEN > sec_debug_extra_info_buf
			 + SZ_1K)
			break;

		ptr += snprintf(ptr, maxlen, ",\"%s\":\"%s\"",
			sec_debug_extra_info_backup->item[i].key,
			sec_debug_extra_info_backup->item[i].val);
	}
}

/******************************************************************************
 * sec_debug_store_extra_info_A
 ******************************************************************************/

void sec_debug_store_extra_info_A(void)
{
	sec_debug_store_extra_info(INFO_AID, INFO_MAX_A);
}

/******************************************************************************
 * sec_debug_store_extra_info_B
 ******************************************************************************/

void sec_debug_store_extra_info_B(void)
{
	sec_debug_store_extra_info(INFO_BID, INFO_MAX_B);
}

/******************************************************************************
 * sec_debug_store_extra_info_C
 ******************************************************************************/

void sec_debug_store_extra_info_C(void)
{
	sec_debug_store_extra_info(INFO_CID, INFO_MAX_C);
}

/******************************************************************************
 * sec_debug_store_extra_info_M
 ******************************************************************************/

void sec_debug_store_extra_info_M(void)
{
	sec_debug_store_extra_info(INFO_MID, INFO_MAX_M);
}

/******************************************************************************
 * sec_debug_set_extra_info_id
******************************************************************************/

static void sec_debug_set_extra_info_id(void)
{
	struct timespec ts;

	getnstimeofday(&ts);

	sec_debug_set_extra_info(INFO_AID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
	sec_debug_set_extra_info(INFO_BID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
	sec_debug_set_extra_info(INFO_CID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
	sec_debug_set_extra_info(INFO_MID, "%09lu%s", ts.tv_nsec, EXTRA_VERSION);
}

/******************************************************************************
 * sec_debug_set_extra_info_ktime
******************************************************************************/

static void sec_debug_set_extra_info_ktime(void)
{
	u64 ts_nsec;

	ts_nsec = local_clock();
	do_div(ts_nsec, 1000000000);
	sec_debug_set_extra_info(INFO_KTIME, "%lu", (unsigned long)ts_nsec);
}

/******************************************************************************
 * sec_debug_set_extra_info_fault
******************************************************************************/

void sec_debug_set_extra_info_fault(unsigned long addr, struct pt_regs *regs)
{
	if (regs) {
		pr_crit("sec_debug_set_extra_info_fault = 0x%lx\n", addr);
		sec_debug_set_extra_info(INFO_FAULT, "0x%lx", addr);
		sec_debug_set_extra_info(INFO_PC, "%pS", regs->pc);
		sec_debug_set_extra_info(INFO_LR, "%pS",
					 compat_user_mode(regs) ?
					  regs->compat_lr : regs->regs[30]);
	}
}

/******************************************************************************
 * sec_debug_set_extra_info_bug
******************************************************************************/

void sec_debug_set_extra_info_bug(const char *file, unsigned int line)
{
	sec_debug_set_extra_info(INFO_BUG, "%s:%u", file, line);
}

void sec_debug_set_extra_info_bug_verbose(unsigned long bugaddr)
{
	sec_debug_set_extra_info(INFO_BUG, "%pS", (u64)bugaddr);
}

/******************************************************************************
 * sec_debug_set_extra_info_panic
******************************************************************************/

void sec_debug_set_extra_info_panic(char *str)
{
	if (unlikely(strstr(str, "\nPC is at")))
		strcpy(strstr(str, "\nPC is at"), "");

	sec_debug_set_extra_info(INFO_PANIC, "%s", str);
}

/******************************************************************************
 * sec_debug_set_extra_info_backtrace
******************************************************************************/

void sec_debug_set_extra_info_backtrace(struct pt_regs *regs)
{
	char buf[64];
	struct stackframe frame;
	int offset = 0;
	int sym_name_len;

	pr_crit("sec_debug_store_backtrace\n");

	if (regs) {
		frame.fp = regs->regs[29];
		//frame.sp = regs->sp;
		frame.pc = regs->pc;
	} else {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		//frame.sp = current_stack_pointer;
		frame.pc = (unsigned long)sec_debug_set_extra_info_backtrace;
	}

	while (1) {
		unsigned long where = frame.pc;
		int ret;

		ret = unwind_frame(NULL, &frame);
		if (ret < 0)
			break;

		snprintf(buf, sizeof(buf), "%pf", (void *)where);
		sym_name_len = strlen(buf);

		if (offset + sym_name_len > MAX_EXTRA_INFO_VAL_LEN)
			break;

		if (offset)
			offset += sprintf((char *)sec_debug_extra_info->item[INFO_STACK].val + offset, ":");

		sprintf((char *)sec_debug_extra_info->item[INFO_STACK].val + offset, "%s", buf);
		offset += sym_name_len;
	}
}

/******************************************************************************
 * sec_debug_set_extra_info_wdt_lastpc
******************************************************************************/

void sec_debug_set_extra_info_wdt_lastpc(unsigned long stackframe[][WDT_FRAME], unsigned int kick, unsigned int check)
{
	int cpu;
	char buf[64];
	int offset = 0;
	int sym_name_len;

	pr_crit("sec_debug_store_wdt_lastpc\n");

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if ((check & (1 << cpu)) && !(kick & (1 << cpu))) {
			if (stackframe[cpu][0] != 0)
				snprintf(buf, sizeof(buf), "(%d)%pS", cpu, (void *)stackframe[cpu][0]);
			else
				snprintf(buf, sizeof(buf), "(%d)0", cpu);
			sym_name_len = strlen(buf);

			if (offset + sym_name_len > MAX_EXTRA_INFO_VAL_LEN)
				break;

			if (offset)
				offset += sprintf((char *)sec_debug_extra_info->item[INFO_KLG].val + offset, ":");
			else
				offset += sprintf((char *)sec_debug_extra_info->item[INFO_KLG].val + offset, "WDTPC:");

			sprintf((char *)sec_debug_extra_info->item[INFO_KLG].val + offset, "%s", buf);
			offset += sym_name_len;
		}
	}
}

/******************************************************************************
 * sec_debug_set_extra_info_dpm_timeout
******************************************************************************/

void sec_debug_set_extra_info_dpm_timeout(char *devname)
{
	sec_debug_set_extra_info(INFO_DPM, "%s", devname);
}

/******************************************************************************
 * sec_debug_set_extra_info_smpl
******************************************************************************/

void sec_debug_set_extra_info_smpl(unsigned int count)
{
	sec_debug_set_extra_info(INFO_SMPL, "0x%x", count & 0x3ff);
}

/******************************************************************************
 * sec_debug_set_extra_info_zswap
******************************************************************************/

void sec_debug_set_extra_info_zswap(char *str)
{
	sec_debug_set_extra_info(INFO_ETC, "%s", str);
}

/******************************************************************************
 * sec_debug_set_extra_info_esr
******************************************************************************/

void sec_debug_set_extra_info_esr(unsigned int esr)
{
	sec_debug_set_extra_info(INFO_ESR, "%s (0x%08x)",
				esr_get_class_string(esr), esr);
}

void sec_debug_finish_extra_info(void)
{
	sec_debug_set_extra_info_ktime();
}

extern unsigned int sec_reset_cnt;
static int sec_debug_reset_rwc_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d", sec_reset_cnt);

	return 0;
}

static int sec_debug_reset_rwc_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_debug_reset_rwc_proc_show, NULL);
}

static const struct file_operations sec_debug_reset_rwc_proc_fops = {
	.open = sec_debug_reset_rwc_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int set_debug_reset_extra_info_proc_show(struct seq_file *m, void *v)
{
	char buf[SZ_1K];

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	if (reset_reason == RR_K || reset_reason == RR_D || reset_reason == RR_P) {
		sec_debug_store_extra_info_A();
		memcpy(buf, sec_debug_extra_info_buf, SZ_1K);
		seq_printf(m, buf);
	} else {
		return -ENOENT;
	}

	return 0;
}

static int sec_debug_reset_extra_info_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, set_debug_reset_extra_info_proc_show, NULL);
}

static const struct file_operations sec_debug_reset_extra_info_proc_fops = {
	.open = sec_debug_reset_extra_info_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init sec_debug_reset_extra_info_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("reset_reason_extra_info",
			S_IWUGO, NULL, &sec_debug_reset_extra_info_proc_fops);
	
	if (!entry)
		return -ENOMEM;
	
	proc_set_size(entry, SZ_1K);

	entry = proc_create("reset_rwc", S_IWUGO, NULL,
						&sec_debug_reset_rwc_proc_fops);

	if (!entry)
		return -ENOMEM;

	sec_debug_set_extra_info_id();

	return 0;
}
device_initcall(sec_debug_reset_extra_info_init);
