/* arch/arm/include/asm/sec/sec-debug.h
 *
 * Copyright (C) 2014 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __SEC_DEBUG_H__
#define __SEC_DEBUG_H__

#include <linux/sched.h>
#include <linux/semaphore.h>
#include <soc/sprd/hardware.h>

extern struct class *sec_class;

#ifdef CONFIG_SEC_DEBUG

struct sec_log_buffer {
	u32	sig;
	u32	start;
	u32	size;
	u32	from;
	u32	to;
	u8	data[0];
};

union sec_debug_level_t {
	struct {
		u16 kernel_fault;
		u16 user_fault;
	} en;
	u32 uint_val;
};

#ifdef CONFIG_ARCH_SCX30G3
#define SEC_DEBUG_MAGIC_OFFSET		0x00001A00
#else
#define SEC_DEBUG_MAGIC_OFFSET		0x00000F80
#endif

#define SEC_DEBUG_MAGIC_VA		(SPRD_IRAM0_BASE + \
						SEC_DEBUG_MAGIC_OFFSET)

#define SPRD_INFORM0			(SEC_DEBUG_MAGIC_VA + 0x00000004)
#define SPRD_INFORM1			(SPRD_INFORM0 + 0x00000004)
#define SPRD_INFORM2			(SPRD_INFORM1 + 0x00000004)
#define SPRD_INFORM3			(SPRD_INFORM2 + 0x00000004)
#define SPRD_INFORM4			(SPRD_INFORM3 + 0x00000004)
#define SPRD_INFORM5			(SPRD_INFORM4 + 0x00000004)
#define SPRD_INFORM6			(SPRD_INFORM5 + 0x00000004)

#define SPRD_CORESIGHT_DBG_BASE		(SPRD_CORESIGHT_BASE + 0x30000)
#define SPRD_CORESIGHT_CPU_OFFSET	0x2000
#define CORESIGHT_DBG(offset, cpu)	(SPRD_CORESIGHT_DBG_BASE + offset \
						+ cpu*SPRD_CORESIGHT_CPU_OFFSET)

#define DBGITR				0x84
#define DBGPCSR				0x84
#define DBGDSCR				0x88
#define DBGDTRTX			0x8C
#define DBGDRCR				0x90
#define DBGLAR				0xFB0

extern union sec_debug_level_t sec_debug_level;
extern int sec_debug_init(void);
extern int sec_debug_magic_init(void);
extern void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y, u32 bpp,
				     u32 frames);
extern void sec_getlog_supply_loggerinfo(void *p_main, void *p_radio,
                                        void *p_events, void *p_system);
extern void sec_getlog_supply_kloginfo(void *klog_buf);
extern void sec_gaf_supply_rqinfo(unsigned short curr_offset,
				  unsigned short rq_offset);
extern void sec_debug_save_pte(void *pte, int task_addr);
extern int sec_debug_dump_stack(void);
extern void sec_debug_backup_ctx(struct pt_regs *regs);
extern void sec_debug_callstack_workaround(void);
extern void sec_debug_emergency_restart_handler(void);

#else /* CONFIG_SEC_DEBUG */

static inline int sec_debug_init(void) { return 0; }
static inline int sec_debug_magic_init(void) { return 0; }
static inline void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y,
					    u32 bpp, u32 frames) { }
static inline void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
					     u32 addr1) { }
static inline void sec_getlog_supply_loggerinfo(void *p_main,
						void *p_radio, void *p_events,
						void *p_system) { }
static inline void sec_getlog_supply_kloginfo(void *klog_buf) { }
static inline void sec_gaf_supply_rqinfo(unsigned short curr_offset,
					 unsigned short rq_offset) { }
static inline void sec_debug_save_pte(void *pte, unsigned int faulttype) { }
static inline int sec_debug_dump_stack(void) { return 0; }
static inline void sec_debug_backup_ctx(struct pt_regs *regs) { }
static inline void sec_debug_callstack_workaround(void) { }
static inline void sec_debug_emergency_restart_handler(void) { }

#endif /* CONFIG_SEC_DEBUG */


struct worker;
struct work_struct;

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
extern void __sec_debug_task_log(int cpu, struct task_struct *task);
extern void __sec_debug_irq_log(unsigned int irq, void *fn, int en);
extern void __sec_debug_work_log(struct worker *worker,
			struct work_struct *work, work_func_t f, int en);
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
extern void __sec_debug_timer_log(unsigned int type, void *fn);
#else
static inline void __sec_debug_timer_log(unsigned int type, void *fn) { }
#endif

static inline void sec_debug_task_log(int cpu, struct task_struct *task)
{
	if (unlikely(sec_debug_level.en.kernel_fault))
		__sec_debug_task_log(cpu, task);
}

static inline void sec_debug_irq_log(unsigned int irq, void *fn, int en)
{
	if (unlikely(sec_debug_level.en.kernel_fault))
		__sec_debug_irq_log(irq, fn, en);
}

static inline void sec_debug_work_log(struct worker *worker,
			struct work_struct *work, work_func_t f, int en)
{
	if (unlikely(sec_debug_level.en.kernel_fault))
		__sec_debug_work_log(worker, work, f, en);
}

#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
static inline void sec_debug_timer_log(unsigned int type, void *fn)
{
	if (unlikely(sec_debug_level.en.kernel_fault))
		__sec_debug_timer_log(type, fn);
}
#else
static inline void sec_debug_timer_log(unsigned int type, void *fn) { }
#endif

#else /* CONFIG_SEC_DEBUG_SCHED_LOG */
static inline void sec_debug_task_log(int cpu, struct task_struct *task) { }
static inline void sec_debug_irq_log(unsigned int irq, void *fn, int en) { }
static inline void sec_debug_work_log(struct worker *worker,
			struct work_struct *work, work_func_t f, int en) { }
static inline void sec_debug_timer_log(unsigned int type, void *fn) { }
static inline void sec_debug_softirq_log(unsigned int irq, void *fn, int en) { }
#endif /* CONFIG_SEC_DEBUG_SCHED_LOG */

#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
extern void sec_debug_irq_last_exit_log(void);
#else
static inline void sec_debug_irq_last_exit_log(void) { }
#endif

#ifdef CONFIG_SEC_DEBUG_SEMAPHORE_LOG
extern void debug_semaphore_init(void);
extern void debug_semaphore_down_log(struct semaphore *sem);
extern void debug_semaphore_up_log(struct semaphore *sem);
extern void debug_rwsemaphore_init(void);
extern void debug_rwsemaphore_down_log(struct rw_semaphore *sem, int dir);
extern void debug_rwsemaphore_up_log(struct rw_semaphore *sem);
#define debug_rwsemaphore_down_read_log(x) \
	debug_rwsemaphore_down_log(x, READ_SEM)
#define debug_rwsemaphore_down_write_log(x) \
	debug_rwsemaphore_down_log(x, WRITE_SEM)
#else /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */
static inline void debug_semaphore_init(void) { }
static inline void debug_semaphore_down_log(struct semaphore *sem) { }
static inline void debug_semaphore_up_log(struct semaphore *sem) { }
static inline void debug_rwsemaphore_init(void) { }
static inline void debug_rwsemaphore_down_read_log(struct rw_semaphore *sem) { }
static inline void debug_rwsemaphore_down_write_log(struct rw_semaphore *sem) { }
static inline void debug_rwsemaphore_up_log(struct rw_semaphore *sem) { }
#endif /* CONFIG_SEC_DEBUG_SEMAPHORE_LOG */

enum sec_debug_aux_log_idx {
	SEC_DEBUG_AUXLOG_CPU_BUS_CLOCK_CHANGE,
	SEC_DEBUG_AUXLOG_LOGBUF_LOCK_CHANGE,
	SEC_DEBUG_AUXLOG_ITEM_MAX,
};

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
extern void sec_debug_aux_log(int idx, char *fmt, ...);
#else
static inline void sec_debug_aux_log(int idx, char *fmt, ...) { }
#endif

#ifdef CONFIG_SEC_DEBUG_CORESIGHT
void sec_coresight_trigger_pagefault_nonstopped(void);
#else
static inline void sec_coresight_trigger_pagefault_nonstopped(void) { }
#endif

#endif /* __SEC_DEBUG_H__ */
