// SPDX-License-Identifier: GPL-2.0
/*
 * dump display
 *
 * Copyright (C) 2021 Huaqin, Inc., Hui Yang
 *
 * The main authors of the dump display code are:
 *
 * Hui Yang <yanghui@huaqin.com>
 */
#ifndef CONFIG_RECORD_CRASH_INFO
#define CONFIG_RECORD_CRASH_INFO
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/export.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/sizes.h>

#include <asm/stack_pointer.h>
#include <asm/stacktrace.h>
#include <asm/sysreg.h>
#include <asm/barrier.h>

extern void dump_backtrace_hq(struct pt_regs *regs, struct task_struct *tsk,const char *loglvl,char *str);

#define CRASH_RECORD_REGION_BASE_ADDR 0x8BB1C000
#define CRASH_RECORD_REGION_SIZE SZ_4K

#define CRASH_RECORD_MAGIC 0x4352494E
#define MAX_CRASH_INFO_DES_SIZE SZ_2K
#define CRASH_INFO_UNKNOWN "UNKNOWN"
#define CRASH_INFO_UNKNOWN_LEN 7

static bool b_crash_record_inited = false;

struct crash_info {
    unsigned int magic;
    char main_reason[MAX_CRASH_INFO_DES_SIZE/4];
    char sub_reason[MAX_CRASH_INFO_DES_SIZE/4];
	char back_trace[MAX_CRASH_INFO_DES_SIZE];
};

struct crash_info __iomem *crash_info_region_base;

void record_backtrace_msg(void)
{
	struct pt_regs *regs = NULL;
	char *trace_msg = NULL;
	struct task_struct *tsk = current;

    trace_msg = (char *)kzalloc(MAX_CRASH_INFO_DES_SIZE, GFP_ATOMIC);
	if (!trace_msg) {
		pr_err("alloc trace_msg memory faild\n");
		return;
	}
    dump_backtrace_hq(regs, tsk, crash_info_region_base->back_trace, trace_msg);
    memcpy_toio(crash_info_region_base->back_trace, trace_msg, MAX_CRASH_INFO_DES_SIZE);
    barrier();
}

void set_crash_info_main_reason(const char *str)
{
    int len = 0;
    if(!b_crash_record_inited){
        pr_err("not inited skip set main reason\n");
        return;
    }

    len = strlen(str);
    if(len > (MAX_CRASH_INFO_DES_SIZE - 1)){
        pr_err("skip set for too long\n");
        return;
    }
    memcpy_toio(crash_info_region_base->main_reason, str, MAX_CRASH_INFO_DES_SIZE/4);
}

void set_crash_info_sub_reason(const char *str)
{
    int len = 0;
    if(!b_crash_record_inited){
        pr_err("not inited skip set main reason\n");
        return;
    }

    len = strlen(str);
    if(len > (MAX_CRASH_INFO_DES_SIZE - 1)){
        pr_err("skip set for too long\n");
        return;
    }
    memcpy_toio(crash_info_region_base->sub_reason, str, MAX_CRASH_INFO_DES_SIZE/4);
}


static int __init crash_record_init(void)
{
    crash_info_region_base = ioremap(CRASH_RECORD_REGION_BASE_ADDR, CRASH_RECORD_REGION_SIZE);
    if (!crash_info_region_base){
        pr_err("error to ioremap crash_record base\n");
        return -1;
    }
    crash_info_region_base->magic = CRASH_RECORD_MAGIC;
    memcpy_toio(crash_info_region_base->main_reason, CRASH_INFO_UNKNOWN, CRASH_INFO_UNKNOWN_LEN);
    memcpy_toio(crash_info_region_base->sub_reason, CRASH_INFO_UNKNOWN, CRASH_INFO_UNKNOWN_LEN);
	memcpy_toio(crash_info_region_base->back_trace, CRASH_INFO_UNKNOWN, CRASH_INFO_UNKNOWN_LEN);

    b_crash_record_inited = true;

    return 0;
}

static void __exit crash_record_exit(void)
{
	pr_err("crash_record exit...\n");
}

device_initcall(crash_record_init);
module_exit(crash_record_exit);
MODULE_LICENSE("GPL");
#endif
