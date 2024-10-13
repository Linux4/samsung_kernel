#ifndef SPRD_DEBUG_H
#define SPRD_DEBUG_H

#include <linux/sched.h>
#include <linux/semaphore.h>



#if defined(CONFIG_SPRD_DEBUG)

#define MAGIC_ADDR			(SPRD_IRAM_BASE + 0x00003f64)
#define SPRD_INFORM0			(MAGIC_ADDR + 0x00000004)
#define SPRD_INFORM1			(SPRD_INFORM0 + 0x00000004)
#define SPRD_INFORM2			(SPRD_INFORM1 + 0x00000004)
#define SPRD_INFORM3			(SPRD_INFORM2 + 0x00000004)
#define SPRD_INFORM4			(SPRD_INFORM3 + 0x00000004)
#define SPRD_INFORM5			(SPRD_INFORM4 + 0x00000004)
#define SPRD_INFORM6			(SPRD_INFORM5 + 0x00000004)

union sprd_debug_level_t {
	struct {
		u16 kernel_fault;
		u16 user_fault;
	} en;
	u32 uint_val;
};

extern union sprd_debug_level_t sprd_debug_level;

extern int sprd_debug_init(void);

extern void sprd_debug_check_crash_key(unsigned int code, int value);

extern void sprd_debug_save_pte(void *pte, int task_addr);

#else
static inline int sprd_debug_init(void)
{
	return 0;
}

static inline void sprd_debug_check_crash_key(unsigned int code, int value)
{
}

void sprd_debug_save_pte(void *pte, unsigned int faulttype )
{
}

#endif

struct worker;
struct work_struct;

#ifdef CONFIG_SPRD_DEBUG_SCHED_LOG
extern void __sprd_debug_task_log(int cpu, struct task_struct *task);
extern void __sprd_debug_irq_log(unsigned int irq, void *fn, int en);
extern void __sprd_debug_work_log(struct worker *worker,
				 struct work_struct *work, work_func_t f);
extern void __sprd_debug_hrtimer_log(struct hrtimer *timer,
			enum hrtimer_restart (*fn) (struct hrtimer *), int en);

static inline void sprd_debug_task_log(int cpu, struct task_struct *task)
{
	if (unlikely(sprd_debug_level.en.kernel_fault))
		__sprd_debug_task_log(cpu, task);
}

static inline void sprd_debug_irq_log(unsigned int irq, void *fn, int en)
{
	if (unlikely(sprd_debug_level.en.kernel_fault))
		__sprd_debug_irq_log(irq, fn, en);
}

static inline void sprd_debug_work_log(struct worker *worker,
				      struct work_struct *work, work_func_t f)
{
	if (unlikely(sprd_debug_level.en.kernel_fault))
		__sprd_debug_work_log(worker, work, f);
}

static inline void sprd_debug_hrtimer_log(struct hrtimer *timer,
			 enum hrtimer_restart (*fn) (struct hrtimer *), int en)
{
#ifdef CONFIG_SPRD_DEBUG_HRTIMER_LOG
	if (unlikely(sprd_debug_level.en.kernel_fault))
		__sprd_debug_hrtimer_log(timer, fn, en);
#endif
}

static inline void sprd_debug_softirq_log(unsigned int irq, void *fn, int en)
{
#ifdef CONFIG_SPRD_DEBUG_SOFTIRQ_LOG
	if (unlikely(sprd_debug_level.en.kernel_fault))
		__sprd_debug_irq_log(irq, fn, en);
#endif
}

#else

static inline void sprd_debug_task_log(int cpu, struct task_struct *task)
{
}

static inline void sprd_debug_irq_log(unsigned int irq, void *fn, int en)
{
}

static inline void sprd_debug_work_log(struct worker *worker,
				      struct work_struct *work, work_func_t f)
{
}

static inline void sprd_debug_hrtimer_log(struct hrtimer *timer,
			 enum hrtimer_restart (*fn) (struct hrtimer *), int en)
{
}

static inline void sprd_debug_softirq_log(unsigned int irq, void *fn, int en)
{
}
#endif



/* klaatu - schedule log */
#ifdef CONFIG_SPRD_DEBUG_SCHED_LOG
#define SCHED_LOG_MAX 1024
#define CP_DEBUG

struct sched_log {
	struct task_log {
		unsigned long long time;
#ifdef CP_DEBUG
		unsigned long sys_cnt;
#endif
		char comm[TASK_COMM_LEN];
		pid_t pid;
	} task[NR_CPUS][SCHED_LOG_MAX];
	struct irq_log {
		unsigned long long time;
#ifdef CP_DEBUG
		unsigned long sys_cnt;
#endif
		int irq;
		void *fn;
		int en;
	} irq[NR_CPUS][SCHED_LOG_MAX];
	struct work_log {
		unsigned long long time;
#ifdef CP_DEBUG
		unsigned long sys_cnt;
#endif
		struct worker *worker;
		struct work_struct *work;
		work_func_t f;
	} work[NR_CPUS][SCHED_LOG_MAX];
	struct hrtimer_log {
		unsigned long long time;
#ifdef CP_DEBUG
		unsigned long sys_cnt;
#endif
		struct hrtimer *timer;
		enum hrtimer_restart (*fn)(struct hrtimer *);
		int en;
	} hrtimers[NR_CPUS][8];
};
#endif				/* CONFIG_SPRD_DEBUG_SCHED_LOG */



#endif /* SPRD_DEBUG_H */
