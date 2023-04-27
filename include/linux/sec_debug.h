/*
* Samsung debugging features for Samsung's SoC's.
*
* Copyright (c) 2019 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/irq.h>

/*
 * SEC DEBUG RAMBASE HANDLING 
 */
extern void secdbg_base_clear_magic_rambase(void);

/*
 * SEC DEBUG PANIC HANDLING 
 */
extern void secdbg_base_panic_handler(void *buf, bool dump);
extern void secdbg_base_post_panic_handler(void);

/*
 * SEC DEBUG LAST KMSG
 */
#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
#define SEC_LKMSG_MAGICKEY 0x0000000a6c6c7546

extern void secdbg_lkmg_store(unsigned char *head_ptr,
		unsigned char *curr_ptr, size_t buf_size);
#else
#define secdbg_lkmg_store(a, b, c)		do {} while(0)
#endif

/*
 * SEC DEBUG MODE
 */
extern int secdbg_mode_enter_upload(void);

/*
 * SEC DEBUG - DEBUG SNAPSHOT BASE HOOKING
 */
enum {
	DSS_KEVENT_TASK,
	DSS_KEVENT_WORK,
	DSS_KEVENT_IRQ,
	DSS_KEVENT_FREQ,
	DSS_KEVENT_IDLE,
	DSS_KEVENT_THRM,
	DSS_KEVENT_ACPM,
	DSS_KEVENT_MFRQ,
};

#define SD_ESSINFO_KEY_SIZE	(32)

struct ess_info_offset {
	char key[SD_ESSINFO_KEY_SIZE];
	unsigned long base;
	unsigned long last;
	unsigned int nr;
	unsigned int size;
	unsigned int per_core;
};

extern unsigned long secdbg_base_get_kevent_index_addr(int type);
extern void secdbg_base_get_kevent_info(struct ess_info_offset *p, int type);

/*
 * SEC DEBUG AUTO COMMENT
 */
#ifdef CONFIG_SEC_DEBUG_AUTO_COMMENT
extern void secdbg_comm_log_disable(int type);
extern void secdbg_comm_log_once(int type);
#endif /* CONFIG_SEC_DEBUG_AUTO_COMMENT */

/*
 * SEC DEBUG EXTRA INFO
 */
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
enum secdbg_exin_fault_type {
	UNDEF_FAULT,
	BAD_MODE_FAULT,
	WATCHDOG_FAULT,
	KERNEL_FAULT,
	MEM_ABORT_FAULT,
	SP_PC_ABORT_FAULT,
	PAGE_FAULT,
	ACCESS_USER_FAULT,
	EXE_USER_FAULT,
	ACCESS_USER_OUTSIDE_FAULT,
	BUG_FAULT,
	SERROR_FAULT,
	SEABORT_FAULT,
	FAULT_MAX,
};

extern void secdbg_exin_set_finish(void);
extern void secdbg_exin_set_fault(enum secdbg_exin_fault_type,
				  unsigned long addr, struct pt_regs *regs);
extern void secdbg_exin_set_bug(const char *file, unsigned int line);
extern void secdbg_exin_set_panic(char *str);
extern void secdbg_exin_set_backtrace(struct pt_regs *regs);
extern void secdbg_exin_set_backtrace_cpu(struct pt_regs *regs, int cpu);
extern void secdbg_exin_set_backtrace_task(struct task_struct *tsk);
extern void secdbg_exin_set_sysmmu(char *str);
extern void secdbg_exin_set_busmon(char *str);
extern void secdbg_exin_set_dpm_timeout(char *devname);
extern void secdbg_exin_set_smpl(unsigned long count);
extern void secdbg_exin_set_esr(unsigned int esr);
extern void secdbg_exin_set_merr(char *merr);
extern void secdbg_exin_set_hint(unsigned long hint);
extern void secdbg_exin_set_decon(unsigned int err);
extern void secdbg_exin_set_batt(int cap, int volt, int temp, int curr);
extern void secdbg_exin_set_ufs_error(char *str);
extern void secdbg_exin_set_zswap(char *str);
extern void secdbg_exin_set_mfc_error(char *str);
extern void secdbg_exin_set_aud(char *str);
extern void secdbg_exin_set_gpuinfo(const char *str);
extern void secdbg_exin_set_epd(char *str);
extern void secdbg_exin_set_unfz(const char *tmp, int pid);
extern char *secdbg_exin_get_unfz(void);
extern void secdbg_exin_set_master_ocp(void);
extern void secdbg_exin_set_slave_ocp(void);
#else /* !CONFIG_SEC_DEBUG_EXTRA_INFO */
#define secdbg_exin_set_finish(a)	do { } while (0)
#define secdbg_exin_set_fault(a, b, c)	do { } while (0)
#define secdbg_exin_set_bug(a, b)	do { } while (0)
#define secdbg_exin_set_panic(a)	do { } while (0)
#define secdbg_exin_set_backtrace(a)	do { } while (0)
#define secdbg_exin_set_backtrace_cpu(a, b)	do { } while (0)
#define secdbg_exin_set_backtrace_task(a)	do { } while (0)
#define secdbg_exin_set_sysmmu(a)	do { } while (0)
#define secdbg_exin_set_busmon(a)	do { } while (0)
#define secdbg_exin_set_dpm_timeout(a)	do { } while (0)
#define secdbg_exin_set_smpl(a)		do { } while (0)
#define secdbg_exin_set_esr(a)		do { } while (0)
#define secdbg_exin_set_merr(a)		do { } while (0)
#define secdbg_exin_set_hint(a)		do { } while (0)
#define secdbg_exin_set_decon(a)	do { } while (0)
#define secdbg_exin_set_batt(a, b, c, d)	do { } while (0)
#define secdbg_exin_set_ufs_error(a)	do { } while (0)
#define secdbg_exin_set_zswap(a)	do { } while (0)
#define secdbg_exin_set_mfc_error(a)	do { } while (0)
#define secdbg_exin_set_aud(a)		do { } while (0)
#define secdbg_exin_set_gpuinfo(a)		do { } while (0)
#define secdbg_exin_set_epd(a)		do { } while (0)
#define secdbg_exin_set_unfz(a)		do { } while (0)
#define secdbg_exin_get_unfz(a)		do { } while (0)
#define secdbg_exin_set_master_ocp()		do { } while (0)
#define secdbg_exin_set_slave_ocp()		do { } while (0)
#endif /* CONFIG_SEC_DEBUG_EXTRA_INFO */

#ifdef CONFIG_SEC_DEBUG_WATCHDOGD_FOOTPRINT
extern void secdbg_wdd_set_keepalive(void);
extern void secdbg_wdd_set_start(void);
extern void secdbg_wdd_set_emerg_addr(unsigned long addr);
#else
#define secdbg_wdd_set_keepalive(a)	do { } while (0)
#define secdbg_wdd_set_start(a)		do { } while (0)
#define secdbg_wdd_set_emerg_addr(a)	do { } while (0)
#endif

#ifdef CONFIG_SEC_DEBUG_FREQ
extern void secdbg_freq_check(int type, unsigned long index, unsigned long freq, int en);
#endif

#ifdef CONFIG_SEC_DEBUG_SYSRQ_KMSG
extern size_t secdbg_hook_get_curr_init_ptr(void);
extern size_t dbg_snapshot_get_curr_ptr_for_sysrq(void);
#endif

/* SEC_LOCKUP_INFO */
extern void secdbg_base_set_task_in_soft_lockup(uint64_t task);
extern void secdbg_base_set_cpu_in_soft_lockup(uint64_t cpu);
extern void secdbg_base_set_task_in_hard_lockup(uint64_t task);
extern void secdbg_base_set_cpu_in_hard_lockup(uint64_t cpu);

/* unfrozen task */
#ifdef CONFIG_SEC_DEBUG_UNFROZEN_TASK
extern void secdbg_base_set_unfrozen_task(uint64_t task);
extern void secdbg_base_set_unfrozen_task_count(uint64_t count);
#else
static inline void secdbg_base_set_unfrozen_task(uint64_t task) {}
static inline void secdbg_base_set_unfrozen_task_count(uint64_t count) {}
#endif /* CONFIG_SEC_DEBUG_UNFROZEN_TASK */

/* CONFIG_SEC_DEBUG_BAD_STACK_INFO */
extern void secdbg_base_set_bs_info_phase(int phase);

#ifdef CONFIG_SEC_DEBUG_DTASK
extern void secdbg_dtsk_print_info(struct task_struct *task, bool raw);
extern void secdbg_dtsk_set_data(int type, void *data);
static inline void secdbg_dtsk_clear_data(void)
{
	secdbg_dtsk_set_data(DTYPE_NONE, NULL);
}
#else
#define secdbg_dtsk_print_info(a, b)		do { } while (0)
#define secdbg_dtsk_set_data(a, b)		do { } while (0)
#define secdbg_dtsk_clear_data()		do { } while (0)
#endif /* CONFIG_SEC_DEBUG_DTASK */

#ifdef CONFIG_SEC_DEBUG_PM_DEVICE_INFO
extern void secdbg_base_set_device_shutdown_timeinfo(uint64_t start, uint64_t end, uint64_t duration, uint64_t func);
extern void secdbg_base_clr_device_shutdown_timeinfo(void);
extern void secdbg_base_set_shutdown_device(const char *fname, const char *dname);
extern void secdbg_base_set_suspend_device(const char *fname, const char *dname);
#else
#define secdbg_base_set_device_shutdown_timeinfo(a, b, c, d)	do { } while (0)
#define secdbg_base_clr_device_shutdown_timeinfo()	do { } while (0)
#define secdbg_base_set_shutdown_device(a, b)		do { } while (0)
#define secdbg_base_set_suspend_device(a, b)		do { } while (0)
#endif /* CONFIG_SEC_DEBUG_PM_DEVICE_INFO */

#ifdef CONFIG_SEC_DEBUG_TASK_IN_STATE_INFO
extern void secdbg_base_set_task_in_pm_suspend(uint64_t task);
extern void secdbg_base_set_task_in_sys_reboot(uint64_t task);
extern void secdbg_base_set_task_in_sys_shutdown(uint64_t task);
extern void secdbg_base_set_task_in_dev_shutdown(uint64_t task);
extern void secdbg_base_set_task_in_sync_irq(uint64_t task, unsigned int irq, const char *name, struct irq_desc *desc);
#else
#define secdbg_base_set_task_in_pm_suspend(a)		do { } while (0)
#define secdbg_base_set_task_in_sys_reboot(a)		do { } while (0)
#define secdbg_base_set_task_in_sys_shutdown(a)		do { } while (0)
#define secdbg_base_set_task_in_dev_shutdown(a)		do { } while (0)
#define secdbg_base_set_task_in_sync_irq(a, b, c, d)	do { } while (0)
#endif /* CONFIG_SEC_DEBUG_PM_DEVICE_INFO */

#ifdef CONFIG_SEC_DEBUG_SHOW_USER_STACK
extern void secdbg_send_sig_debuggerd(struct task_struct *p, int opt);
#else
#define secdbg_send_sig_debuggerd(a, b)		do { } while (0)
#endif

#ifdef CONFIG_SEC_DEBUG
#define SDBG_KNAME_LEN	64

struct secdbg_member_type {
	char member[SDBG_KNAME_LEN];
	uint16_t size;
	uint16_t offset;
	uint16_t unused[2];
};

#define SECDBG_DEFINE_MEMBER_TYPE(key, st, mem)					\
	const struct secdbg_member_type sdbg_##key				\
		__attribute__((__section__(".secdbg_mbtab." #key))) = {		\
		.member = #key,							\
		.size = FIELD_SIZEOF(struct st, mem),				\
		.offset = offsetof(struct st, mem),				\
	}
#else
#define SECDBG_DEFINE_MEMBER_TYPE(a, b, c)
#endif /* CONFIG_SEC_DEBUG */

#ifdef CONFIG_SEC_DEBUG_SOFTDOG
extern void secdbg_softdog_show_info(void);
#else
#define secdbg_softdog_show_info()		do { } while (0)
#endif

#endif /* SEC_DEBUG_H */
