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
#ifdef CONFIG_RECORD_CRASH_INFO
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/sizes.h>

#include <asm/stacktrace.h>
#include <asm/sysreg.h>
#include <asm/barrier.h>

#define CRASH_RECORD_REGION_BASE_ADDR 0x8B717400
#define CRASH_RECORD_REGION_SIZE SZ_4K

#define CRASH_RECORD_MAGIC 0x4352494E
#define MAX_CRASH_INFO_DES_SIZE SZ_2K
#define CRASH_INFO_UNKNOWN "UNKNOWN"
#define CRASH_INFO_UNKNOWN_LEN 7

static bool b_crash_record_inited = false;

/* F52 code for QN3979-1862 by yutao at 2021/04/27 start */
#ifdef HQ_FACTORY_BUILD
extern u32 *log_first_idx_addr_get(void);
extern u32 *log_next_idx_addr_get(void);

struct mini_symbols{
    void * swapper_pg_dir;
    u32 * log_first_idx_addr;
    u32 * log_next_idx_addr;
    char *log_buf;
};
#endif

struct crash_info {
    unsigned int magic;
    char main_reason[MAX_CRASH_INFO_DES_SIZE/4];
    char sub_reason[MAX_CRASH_INFO_DES_SIZE/4];
	char back_trace[MAX_CRASH_INFO_DES_SIZE];
#ifdef HQ_FACTORY_BUILD
    struct mini_symbols symbols;
#endif
};

struct crash_info __iomem *crash_info_region_base;

#ifdef HQ_FACTORY_BUILD
void collect_mini_symbols(void)
{
    struct mini_symbols *symbols;
    if (crash_info_region_base == NULL) {
        return;
    }

    symbols = &(crash_info_region_base->symbols);
    symbols->swapper_pg_dir = swapper_pg_dir;
    symbols->log_first_idx_addr = log_first_idx_addr_get();
    symbols->log_next_idx_addr = log_next_idx_addr_get();
    symbols->log_buf = log_buf_addr_get();
}
#endif
/* F52 code for QN3979-1862 by yutao at 2021/04/27 end */

void record_backtrace_msg(void)
{
	struct stackframe frame;
	struct pt_regs *regs = NULL;
	int skip = 0;
	long cur_state = 0;
	char *trace_msg = NULL;
	char sym[KSYM_SYMBOL_LEN];
	unsigned long offset, size;
	struct task_struct *tsk = current;

	if (!try_get_task_stack(tsk))
		return;

    trace_msg = (char*)kzalloc(MAX_CRASH_INFO_DES_SIZE, GFP_ATOMIC);
	if (!trace_msg) {
		pr_err("alloc trace_msg memory faild\n");
		return;
	}

	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.pc = (unsigned long)record_backtrace_msg;

	do {
		if (tsk != current && (cur_state != tsk->state)) {
			printk("The task:%s had been rescheduled!\n",
				tsk->comm);
			break;
		}
		/* skip until specified stack frame */
		if (!skip) {
			kallsyms_lookup(frame.pc, &size, &offset, NULL, sym);
			sprintf(trace_msg, "%s\n\r%s", trace_msg, sym);
		} else if (frame.fp == regs->regs[29]) {
			skip = 0;
			kallsyms_lookup(regs->pc, &size, &offset, NULL, sym);
			sprintf(trace_msg, "%s\n\r%s", trace_msg, sym);
		}
	} while (!unwind_frame(tsk, &frame));

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
    pr_info("%s init\n", __func__);
    crash_info_region_base = ioremap(CRASH_RECORD_REGION_BASE_ADDR, CRASH_RECORD_REGION_SIZE);
    if (!crash_info_region_base){
        pr_err("error to ioremap crash_record base\n");
        return -1;
    }

    crash_info_region_base->magic = CRASH_RECORD_MAGIC;
    memcpy_toio(crash_info_region_base->main_reason, CRASH_INFO_UNKNOWN, CRASH_INFO_UNKNOWN_LEN);
    memcpy_toio(crash_info_region_base->sub_reason, CRASH_INFO_UNKNOWN, CRASH_INFO_UNKNOWN_LEN);
	memcpy_toio(crash_info_region_base->back_trace, CRASH_INFO_UNKNOWN, CRASH_INFO_UNKNOWN_LEN);
/* F52 code for QN3979-1862 by yutao at 2021/04/27 start */
#ifdef HQ_FACTORY_BUILD
    collect_mini_symbols();
#endif
/* F52 code for QN3979-1862 by yutao at 2021/04/27 end */

    b_crash_record_inited = true;

    return 0;
}

device_initcall(crash_record_init);
#endif
