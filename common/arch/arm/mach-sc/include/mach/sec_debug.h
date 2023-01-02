/* include/mach/sec-debug.h
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


#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/sched.h>
#include <linux/semaphore.h>
#include <mach/hardware.h>

#ifdef CONFIG_SEC_DEBUG

struct sec_log_buffer {
	u32	sig;
	u32	start;
	u32	size;
	u8	data[0];
};

union sec_debug_level_t {
	struct {
		u16 kernel_fault;
		u16 user_fault;
	} en;
	u32 uint_val;
};

#ifdef CONFIG_ARCH_SCX15

#define SEC_DEBUG_MAGIC_VA      (SEC_DEBUG_MAGIC_BASE_VA)
#define SPRD_INFORM0			(SEC_DEBUG_MAGIC_VA + 0x00000004)
#define SPRD_INFORM1			(SPRD_INFORM0 + 0x00000004)
#define SPRD_INFORM2			(SPRD_INFORM1 + 0x00000004)
#define SPRD_INFORM3			(SPRD_INFORM2 + 0x00000004)
#define SPRD_INFORM4			(SPRD_INFORM3 + 0x00000004)
#define SPRD_INFORM5			(SPRD_INFORM4 + 0x00000004)
#define SPRD_INFORM6			(SPRD_INFORM5 + 0x00000004)

#else

#define SEC_DEBUG_MAGIC_VA		(SPRD_IRAM0_BASE + 0x00000f80)
#define SPRD_INFORM0			(SEC_DEBUG_MAGIC_VA + 0x00000004)
#define SPRD_INFORM1			(SPRD_INFORM0 + 0x00000004)
#define SPRD_INFORM2			(SPRD_INFORM1 + 0x00000004)
#define SPRD_INFORM3			(SPRD_INFORM2 + 0x00000004)
#define SPRD_INFORM4			(SPRD_INFORM3 + 0x00000004)
#define SPRD_INFORM5			(SPRD_INFORM4 + 0x00000004)
#define SPRD_INFORM6			(SPRD_INFORM5 + 0x00000004)

#endif

extern union sec_debug_level_t sec_debug_level;

extern int sec_debug_init(void);

extern int sec_debug_magic_init(void);

extern void sec_debug_check_crash_key(unsigned int code, int value);

extern void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y, u32 bpp,
				     u32 frames);
extern void sec_getlog_supply_loggerinfo(void *p_main, void *p_radio,
                                        void *p_events, void *p_system);

extern void sec_getlog_supply_kloginfo(void *klog_buf);

extern void sec_gaf_supply_rqinfo(unsigned short curr_offset,
				  unsigned short rq_offset);

extern void sec_debug_save_pte(void *pte, int task_addr);

int sec_debug_dump_stack(void);
#else
static inline int sec_debug_init(void)
{
	return 0;
}

static inline int sec_debug_magic_init(void)
{
	return 0;
}

static inline void sec_debug_check_crash_key(unsigned int code, int value)
{
}

static inline void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y,
					    u32 bpp, u32 frames)
{
}

static inline void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1,
					     u32 addr1)
{
}

static inline void sec_getlog_supply_loggerinfo(void *p_main,
						void *p_radio, void *p_events,
						void *p_system)
{
}

static inline void sec_getlog_supply_kloginfo(void *klog_buf)
{
}

static inline void sec_gaf_supply_rqinfo(unsigned short curr_offset,
					 unsigned short rq_offset)
{
}

void sec_debug_save_pte(void *pte, unsigned int faulttype);
{
}

#endif

struct worker;
struct work_struct;

#ifdef CONFIG_SEC_DEBUG_SCHED_LOG
extern void __sec_debug_task_log(int cpu, struct task_struct *task);
extern void __sec_debug_irq_log(unsigned int irq, void *fn, int en);
extern void __sec_debug_work_log(struct worker *worker,
			struct work_struct *work, work_func_t f, int en);
#ifdef CONFIG_SEC_DEBUG_TIMER_LOG
extern void __sec_debug_timer_log(unsigned int type, void *fn);
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
static inline void sec_debug_timer_log(unsigned int type, void *fn)
{
}
#endif

#ifdef CONFIG_SEC_DEBUG_SOFTIRQ_LOG
static inline void sec_debug_softirq_log(unsigned int irq, void *fn, int en)
{
	if (unlikely(sec_debug_level.en.kernel_fault))
#else
static inline void sec_debug_softirq_log(unsigned int irq, void *fn, int en)
{
}
#endif

#else
static inline void sec_debug_task_log(int cpu, struct task_struct *task)
{
}

static inline void sec_debug_irq_log(unsigned int irq, void *fn, int en)
{
}

static inline void sec_debug_work_log(struct worker *worker,
			      struct work_struct *work, work_func_t f, int en)
{
}

static inline void sec_debug_timer_log(unsigned int type, void *fn)
{
}

static inline void sec_debug_softirq_log(unsigned int irq, void *fn, int en)
{
}
#endif
#ifdef CONFIG_SEC_DEBUG_IRQ_EXIT_LOG
extern void sec_debug_irq_last_exit_log(void);
#else
static inline void sec_debug_irq_last_exit_log(void)
{
}
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
#else
static inline void debug_semaphore_init(void)
{
}

static inline void debug_semaphore_down_log(struct semaphore *sem)
{
}

static inline void debug_semaphore_up_log(struct semaphore *sem)
{
}

static inline void debug_rwsemaphore_init(void)
{
}

static inline void debug_rwsemaphore_down_read_log(struct rw_semaphore *sem)
{
}

static inline void debug_rwsemaphore_down_write_log(struct rw_semaphore *sem)
{
}

static inline void debug_rwsemaphore_up_log(struct rw_semaphore *sem)
{
}
#endif

enum sec_debug_aux_log_idx {
	SEC_DEBUG_AUXLOG_CPU_BUS_CLOCK_CHANGE,
	SEC_DEBUG_AUXLOG_LOGBUF_LOCK_CHANGE,
	SEC_DEBUG_AUXLOG_ITEM_MAX,
};

#ifdef CONFIG_SEC_DEBUG_AUXILIARY_LOG
extern void sec_debug_aux_log(int idx, char *fmt, ...);
#else
#define sec_debug_aux_log(idx, ...) do { } while (0)
#endif

#endif /* SEC_DEBUG_H */
