#ifndef SEC_LOG64_H
#define SEC_LOG64_H

#include <linux/sched.h>
#include <linux/semaphore.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sec_debug64.h>

struct sec_log_buffer {
	unsigned long sig;
	unsigned long start;
	unsigned long size;
	unsigned long from;
	unsigned long to;
	u8	data[0];
};

#define SCHED_LOG_MAX 2048

struct sched_log {
	struct task_log {
	unsigned long long time;
			char comm[TASK_COMM_LEN];
			pid_t pid;
	} task[CONFIG_NR_CPUS][SCHED_LOG_MAX];
		struct irq_log {
		unsigned long long time;
			int irq;
			void *fn;
			int en;
	} irq[CONFIG_NR_CPUS][SCHED_LOG_MAX];
		struct work_log {
		unsigned long long time;
			struct worker *worker;
			struct work_struct *work;
			work_func_t f;
			int en;
	} work[CONFIG_NR_CPUS][SCHED_LOG_MAX];
	struct timer_log {
		unsigned long long time;
		unsigned int type;
		void *fn;
	} timer[CONFIG_NR_CPUS][SCHED_LOG_MAX];
};

extern struct sched_log *psec_debug_log;

struct worker;
struct work_struct;

extern void __sec_debug_task_log(int cpu, struct task_struct *task);
extern void __sec_debug_irq_log(unsigned int irq, void *fn, int en);
extern void __sec_debug_work_log(struct worker *worker,
			struct work_struct *work, work_func_t f, int en);
extern void __sec_debug_timer_log(unsigned int type, void *fn);

extern union sec_debug_level_t sec_debug_level;

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

static inline void sec_debug_timer_log(unsigned int type, void *fn)
{
	if (unlikely(sec_debug_level.en.kernel_fault))
		__sec_debug_timer_log(type, fn);
}

enum sec_debug_aux_log_idx {
	SEC_DEBUG_AUXLOG_CPU_BUS_CLOCK_CHANGE,
	SEC_DEBUG_AUXLOG_LOGBUF_LOCK_CHANGE,
	SEC_DEBUG_AUXLOG_ITEM_MAX,
};

extern void sec_getlog_supply_fbinfo(void *p_fb, u32 res_x, u32 res_y, u32 bpp,
u32 frames);
extern void sec_getlog_supply_loggerinfo(void *p_main, void *p_radio,
   void *p_events, void *p_system);

extern void sec_getlog_supply_kloginfo(void *klog_buf);

#endif //SEC_LOG64_H
