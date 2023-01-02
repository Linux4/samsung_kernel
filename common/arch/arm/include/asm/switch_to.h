#ifndef __ASM_ARM_SWITCH_TO_H
#define __ASM_ARM_SWITCH_TO_H

#include <linux/thread_info.h>

/*
 * switch_to(prev, next) should switch from task `prev' to `next'
 * `prev' will never be the same as `next'.  schedule() itself
 * contains the memory barrier to tell GCC not to cache `current'.
 */
extern struct task_struct *__switch_to(struct task_struct *, struct thread_info *, struct thread_info *);
#if defined(CONFIG_SPRD_CPU_RATE) && defined(CONFIG_SPRD_DEBUG)
extern void update_cpu_usage(struct task_struct *prev, struct task_struct *next);
#define sprd_update_cpu_usage(prev, next)	update_cpu_usage(prev, next)
#else
#define sprd_update_cpu_usage(prev, next)
#endif

#define switch_to(prev,next,last)					\
do {									\
	sprd_update_cpu_usage(prev, next);				\
	last = __switch_to(prev,task_thread_info(prev), task_thread_info(next));	\
} while (0)

#endif /* __ASM_ARM_SWITCH_TO_H */
