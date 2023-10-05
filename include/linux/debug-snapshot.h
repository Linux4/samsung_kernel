/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos-SnapShot debugging framework for Exynos SoC
 *
 * Author: Hosung Kim <Hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef DEBUG_SNAPSHOT_H
#define DEBUG_SNAPSHOT_H

#ifdef CONFIG_DEBUG_SNAPSHOT
#include <asm/ptrace.h>
#include <linux/bug.h>
#include "debug-snapshot-binder.h"
#include <dt-bindings/soc/samsung/debug-snapshot-def.h>

/* mandatory */
extern void dbg_snapshot_task(int cpu, void *v_task);
extern void dbg_snapshot_work(void *worker, void *v_task, void *fn, int en);
extern void dbg_snapshot_cpuidle(char *modes, unsigned state, int diff, int en);
extern void dbg_snapshot_suspend(char *log, void *fn, void *dev, int state, int en);
extern void dbg_snapshot_irq(int irq, void *fn, void *val, unsigned long long time, int en);
extern void dbg_snapshot_print_notifier_call(void **nl, unsigned long func, int en);
extern int dbg_snapshot_try_enable(const char *name, unsigned long long duration);
extern int dbg_snapshot_set_enable(int en);
extern int dbg_snapshot_set_enable_item(const char *name, int en);
extern int dbg_snapshot_get_enable(void);
extern int dbg_snapshot_get_enable_item(const char *name);
extern int dbg_snapshot_set_dpm_item(char *first, char *second, char *node, int policy, int value);
extern int dbg_snapshot_get_dpm_item(char *first, char *second, char *node);
extern int dbg_snapshot_get_dpm_item_policy(char *first, char *second, char *node);
extern int dbg_snapshot_get_dpm_item_value(char *first, char *second, char *node);
extern void dbg_snapshot_soc_do_dpm_policy(int policy);
extern int dbg_snapshot_save_context(void *regs);
extern int dbg_snapshot_save_reg(void *regs);
extern void dbg_snapshot_save_system(void *unused);
extern int dbg_snapshot_dump_panic(char *str, size_t len);
extern int dbg_snapshot_early_panic(void);
extern int dbg_snapshot_prepare_panic(void);
extern int dbg_snapshot_post_panic(void);
extern int dbg_snapshot_post_reboot(char *cmd);
extern int dbg_snapshot_set_hardlockup(int);
extern int dbg_snapshot_get_hardlockup(void);
extern unsigned int dbg_snapshot_get_item_size(char *name);
extern unsigned int dbg_snapshot_get_item_paddr(char *name);
extern unsigned long dbg_snapshot_get_item_vaddr(char *name);
extern unsigned long dbg_snapshot_get_item_curr_ptr(char *name);
extern void dbg_snapshot_set_sjtag_status(void);
extern int dbg_snapshot_get_sjtag_status(void);
extern int dbg_snapshot_get_debug_level(void);
extern int dbg_snapshot_get_debug_level_reg(void);
extern bool dbg_snapshot_dumper_one(void *, char *, size_t, size_t *);
extern void dbg_snapshot_panic_handler_safe(void);
extern void dbg_snapshot_set_core_pmu_val(unsigned int val, unsigned int cpu);
extern unsigned int dbg_snapshot_get_core_pmu_val(unsigned int cpu);
extern void dbg_snapshot_set_core_ehld_stat(unsigned int val, unsigned int cpu);
extern unsigned int dbg_snapshot_get_core_ehld_stat(unsigned int cpu);
extern unsigned long dbg_snapshot_get_spare_vaddr(unsigned int offset);
extern unsigned long dbg_snapshot_get_spare_paddr(unsigned int offset);
extern unsigned long dbg_snapshot_get_last_pc(unsigned int cpu);
extern unsigned long dbg_snapshot_get_last_pc_paddr(void);
extern void dbg_snapshot_hook_hardlockup_entry(void *v_regs);
extern void dbg_snapshot_hook_hardlockup_exit(void);
extern void dbg_snapshot_dump_sfr(void);
extern int dbg_snapshot_hook_pmsg(char *buffer, size_t count);
extern void dbg_snapshot_save_log(int cpu, unsigned long where);
extern int dbg_snapshot_add_bl_item_info(const char *name,
		unsigned int paddr, unsigned int size);
extern void dbg_snapshot_scratch_clear(void);
extern int dbg_snapshot_is_hardlockup(void);
extern void dbg_snapshot_early_init_log_enabled(const char *name, int en);
extern int dbg_snapshot_early_init_dt_scan_dpm(unsigned long node, const char *uname, int depth, void *data);
extern void dbg_snapshot_do_dpm_policy(unsigned int policy);

struct clk;
extern void dbg_snapshot_clk(void *clock, const char *func_name, unsigned long arg, int mode);
extern void dbg_snapshot_regulator(unsigned long long timestamp, char *f_name,
				unsigned int addr, unsigned int volt, unsigned int rvolt, int en);
extern void dbg_snapshot_acpm(unsigned long long timestamp, const char *log, unsigned int data);
extern void dbg_snapshot_thermal(void *data, unsigned int temp, char *name, unsigned long long max_cooling);
extern void dbg_snapshot_hrtimer(void *timer, s64 *now, void *fn, int en);
extern void dbg_snapshot_pmu(int id, const char *func_name, int mode);
extern void dbg_snapshot_freq(int type, unsigned long old_freq, unsigned long target_freq, int en);
extern void dbg_snapshot_freq_misc(int type, unsigned long old_freq, unsigned long target_freq, int en);
extern void dbg_snapshot_dm(int type, unsigned long min, unsigned long max, s32 wait_t, s32 t);

struct i2c_adapter;
struct i2c_msg;
extern void dbg_snapshot_i2c(struct i2c_adapter *adap, struct i2c_msg *msgs, int num, int en);

struct spi_controller;
struct spi_message;
extern void dbg_snapshot_spi(struct spi_controller *ctlr, struct spi_message *cur_msg, int en);

#define dbg_snapshot_irq_var(v)			do { v = cpu_clock(raw_smp_processor_id()); } while (0)

#ifdef CONFIG_DEBUG_SNAPSHOT_BINDER
extern void dbg_snapshot_binder(struct trace_binder_transaction_base *base,
					struct trace_binder_transaction *transaction,
					struct trace_binder_transaction_error *error);
#else
#define dbg_snapshot_binder(a, b, c)		do { } while (0)
#endif

extern void __dbg_snapshot_printk(const char *fmt, ...);
#ifndef CONFIG_DEBUG_SNAPSHOT_USER_MODE
extern void dbg_snapshot_printk(const char *fmt, ...);
extern void dbg_snapshot_printkl(size_t msg, size_t val);
#else
#define dbg_snapshot_printk(...)		do { } while (0)
#define dbg_snapshot_printkl(a, b)		do { } while (0)
#endif


#ifdef CONFIG_DEBUG_SNAPSHOT_IRQ_DISABLED
extern void dbg_snapshot_irqs_disabled(unsigned long flags);
#else
#define dbg_snapshot_irqs_disabled(a)	do { } while (0)
#endif

#ifdef CONFIG_DEBUG_SNAPSHOT_REG
extern void dbg_snapshot_reg(char io_type, char data_type, void *addr);
#else
#define dbg_snapshot_reg(a, b, c)		do { } while (0)
#endif

#ifdef CONFIG_DEBUG_SNAPSHOT_SPINLOCK
extern void dbg_snapshot_spinlock(void *lock, int en);
#else
#define dbg_snapshot_spinlock(a, b)		do { } while (0)
#endif

void dbg_snapshot_check_crash_key(unsigned int code, int value);

#ifdef CONFIG_OF_RESERVED_MEM
extern int dbg_snapshot_reserved_mem_check(unsigned long node, unsigned long size);
#else
static inline int dbg_snapshot_reserved_mem_check(unsigned long node, unsigned long size)
{
	return 0;
}
#endif

#ifdef CONFIG_SEC_DEBUG_LOCKUP_INFO
extern void secdbg_hardlockup_get_info(unsigned int cpu, void *info);
extern void secdbg_softlockup_get_info(unsigned int cpu, void *info);
#else
#define secdbg_hardlockup_get_info(a, b)	do { } while (0)
#define secdbg_softlockup_get_info(a, b)	do { } while (0)
#endif

#ifdef CONFIG_SEC_DEBUG_COMPLETE_HINT
extern u64 secdbg_snapshot_get_hardlatency_info(unsigned int cpu);
#else
#define secdbg_snapshot_get_hardlatency_info(a)	do { } while (0)
#endif


#ifdef CONFIG_SEC_DEBUG_WQ_LOCKUP_INFO
extern void secdbg_show_sched_info(unsigned int cpu, int count);
extern int secdbg_show_busy_task(unsigned int cpu, unsigned long long duration, int count);
extern struct task_struct *get_the_busiest_task(void);
#else
#define secdbg_show_sched_info(a, b)	do { } while (0)
static inline int secdbg_show_busy_task(unsigned int cpu, unsigned long long duration, int count)
{
        return -1;
}

static struct task_struct *get_the_busiest_task(void)
{
	return NULL;
}
#endif

#else /* CONFIG_DEBUG_SNAPSHOT */
#define dbg_snapshot_acpm(a,b,c)		do { } while(0)
#define dbg_snapshot_task(a,b)			do { } while(0)
#define dbg_snapshot_work(a,b,c,d)		do { } while(0)
#define dbg_snapshot_clockevent(a,b,c)		do { } while(0)
#define dbg_snapshot_cpuidle(a,b,c,d)		do { } while(0)
#define dbg_snapshot_suspend(a,b,c,d,e)		do { } while(0)
#define dbg_snapshot_regulator(a,b,c,d,e,f)	do { } while(0)
#define dbg_snapshot_thermal(a,b,c,d)		do { } while(0)
#define dbg_snapshot_irq(a,b,c,d,e)		do { } while(0)
#define dbg_snapshot_irqs_disabled(a)		do { } while(0)
#define dbg_snapshot_spi(a,b,c)			do { } while(0)
#define dbg_snapshot_spinlock(a,b)		do { } while(0)
#define dbg_snapshot_clk(a,b,c,d)		do { } while(0)
#define dbg_snapshot_pmu(a,b,c)			do { } while(0)
#define dbg_snapshot_freq(a,b,c,d)		do { } while(0)
#define dbg_snapshot_freq_misc(a,b,c,d)		do { } while(0)
#define dbg_snapshot_irq_var(v)			do { v = 0; } while(0)
#define dbg_snapshot_reg(a, b, c)		do { } while (0)
#define dbg_snapshot_hrtimer(a,b,c,d)		do { } while(0)
#define dbg_snapshot_i2c(a,b,c,d)		do { } while(0)
#define dbg_snapshot_hook_pmsg(a,b)		do { } while(0)
#define dbg_snapshot_printk(...)		do { } while(0)
#define dbg_snapshot_printkl(a,b)		do { } while(0)
#define dbg_snapshot_save_context(a)		do { } while(0)
#define dbg_snapshot_try_enable(a,b)		do { } while(0)
#define dbg_snapshot_set_enable_item(a, b)	do { } while (0)
#define dbg_snapshot_set_enable(a,b)		do { } while(0)
#define dbg_snapshot_get_enable_item(a)		do { } while (0)
#define dbg_snapshot_get_enable(a)		do { } while(0)
#define dbg_snapshot_set_dpm_item(a, b, c, d, e)	do { } while (0)
#define dbg_snapshot_save_reg(a)		do { } while(0)
#define dbg_snapshot_save_system(a)		do { } while(0)
#define dbg_snapshot_dump_panic(a,b)		do { } while(0)
#define dbg_snapshot_dump_sfr()			do { } while(0)
#define dbg_snapshot_early_panic(a)		do { } while(0)
#define dbg_snapshot_prepare_panic()		do { } while(0)
#define dbg_snapshot_post_panic()		do { } while(0)
#define dbg_snapshot_post_reboot(a)		do { } while(0)
#define dbg_snapshot_set_hardlockup(a)		do { } while(0)
#define dbg_snapshot_get_hardlockup()		do { } while(0)
#define dbg_snapshot_set_sjtag_status() do { } while (0)
#define dbg_snasshot_get_sjtag_status() do { } while (0)
#define dbg_snapshot_get_debug_level()		do { } while(0)
#define dbg_snapshot_get_debug_level_reg()	do { } while (0)
#define dbg_snapshot_set_core_pmu_val(a,b)	do { } while (0)
#define dbg_snapshot_set_core_ehld_stat(a,b)	do { } while (0)
#define dbg_snapshot_check_crash_key(a,b)	do { } while(0)
#define dbg_snapshot_dm(a,b,c,d,e)		do { } while(0)
#define dbg_snapshot_panic_handler_safe()	do { } while(0)
#define dbg_snapshot_get_last_pc(a)		do { } while(0)
#define dbg_snapshot_get_last_pc_paddr()	do { } while(0)
#define dbg_snapshot_hook_hardlockup_entry(a)	do { } while(0)
#define dbg_snapshot_hook_hardlockup_exit()	do { } while(0)
#define dbg_snapshot_binder(a,b,c)		do { } while(0)
#define dbg_snapshot_print_notifier_call(a,b,c)	do { } while(0)
#define dbg_snapshot_save_log(a,b)		do { } while(0)
#define dbg_snapshot_scratch_clear()		do { } while(0)

static inline int dbg_snapshot_get_dpm_item_policy(char *first, char *second, char *node)
{
	return -1;
}
static inline int dbg_snapshot_get_dpm_item_value(char *first, char *second, char *node)
{
	return -1;
}
#ifndef CONFIG_UML
static inline int dbg_snapshot_get_hardlockup(void)
{
	return 0;
}
#endif

#define secdbg_hardlockup_get_info(a, b)	do { } while (0)
#define secdbg_softlockup_get_info(a, b)	do { } while (0)
#define secdbg_snapshot_get_hardlatency_info(a)	do { } while (0)

static inline unsigned int dbg_snapshot_get_item_size(char *name)
{
	return 0;
}
static inline unsigned int dbg_snapshot_get_item_paddr(char *name)
{
	return 0;
}
static inline unsigned long dbg_snapshot_get_item_vaddr(char *name)
{
	return 0;
}
static inline unsigned long dbg_snapshot_get_item_curr_ptr(char *name)
{
	return 0;
}
static inline bool dbg_snapshot_dumper_one(void *v_dumper,
				char *line, size_t size, size_t *len)
{
	return false;
}
static inline int dbg_snapshot_add_bl_item_info(const char *name,
		unsigned int paddr, unsigned int size)
{
	return -1;
}
static inline int dbg_snapshot_is_hardlockup(void)
{
	return 0;
}
static inline unsigned int dbg_snapshot_get_core_pmu_val(unsigned int cpu)
{
	return 0;
}
static inline unsigned int dbg_snapshot_get_core_ehld_stat(unsigned int cpu)
{
	return 0;
}
static inline int dbg_snapshot_reserved_mem_check(unsigned long node, unsigned long size)
{
	return 0;
}

#endif /* CONFIG_DEBUG_SNAPSHOT */
extern void dbg_snapshot_soc_helper_init(void);
static inline void dbg_snapshot_bug_func(void) {BUG();}
#ifdef CONFIG_UML
static inline void dbg_snapshot_spin_func(void) {}
#else
static inline void dbg_snapshot_spin_func(void) {do {wfi();} while(1);}
#endif

extern struct atomic_notifier_head restart_handler_list;
extern struct blocking_notifier_head reboot_notifier_list;
extern struct blocking_notifier_head pm_chain_head;
#ifdef CONFIG_EXYNOS_ITMON
extern struct atomic_notifier_head itmon_notifier_list;
#endif

/**
 * dsslog_flag - added log information supported.
 * @DSS_FLAG_REQ: Generally, marking starting request something
 * @DSS_FLAG_IN: Generally, marking into the function
 * @DSS_FLAG_ON: Generally, marking the status not in, not out
 * @DSS_FLAG_OUT: Generally, marking come out the function
 * @DSS_FLAG_SOFTIRQ: Marking to pass the softirq function
 * @DSS_FLAG_SOFTIRQ_HI_TASKLET: Marking to pass the tasklet function
 * @DSS_FLAG_SOFTIRQ_TASKLET: Marking to pass the tasklet function
 */
enum dsslog_flag {
	DSS_FLAG_REQ			= 0,
	DSS_FLAG_IN			= 1,
	DSS_FLAG_ON			= 2,
	DSS_FLAG_OUT			= 3,
	DSS_FLAG_SOFTIRQ		= 10000,
	DSS_FLAG_SOFTIRQ_HI_TASKLET	= 10100,
	DSS_FLAG_SOFTIRQ_TASKLET	= 10200,
	DSS_FLAG_CALL_TIMER_FN		= 20000,
	DSS_FLAG_SMP_CALL_FN		= 30000,
};

enum dsslog_freq_flag {
	DSS_FLAG_LIT = 0,
	DSS_FLAG_MID,
	DSS_FLAG_BIG,
	DSS_FLAG_INT,
	DSS_FLAG_MIF,
	DSS_FLAG_ISP,
	DSS_FLAG_DISP,
	DSS_FLAG_INTCAM,
	DSS_FLAG_AUD,
	DSS_FLAG_DSP,
	DSS_FLAG_DNC,
	DSS_FLAG_MFC,
	DSS_FLAG_NPU,
	DSS_FLAG_TNR,
	DSS_FLAG_G3D,
	DSS_FLAG_END
};

#define DSS_DEBUG_LEVEL_PREFIX	(0xDB9 << 16)
#define DSS_DEBUG_LEVEL_LOW	(0)
#define DSS_DEBUG_LEVEL_MID	(1)

#endif
