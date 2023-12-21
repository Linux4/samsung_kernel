#ifndef __ASM_MCS_LOCK_H
#define __ASM_MCS_LOCK_H

#ifdef CONFIG_SMP
#include <asm/spinlock.h>

static inline void __wfe(void)
{
	asm volatile(
		"wfe\n"
	);
}
static inline void __sevl(void)
{
	asm volatile(
		"sevl\n"
		);
}

static inline void __sev(void)
{
	asm volatile(
		"sev\n"
		);
}

#define arch_mcs_spin_lock_contended(lock)				\
do {									\
	while(!(smp_load_acquire_exclusive(lock)))			\
		__wfe();						\
} while (0)								\


#define arch_mcs_spin_unlock_contended(lock)				\
do {									\
	smp_store_release(lock, 1);					\
} while (0)

#endif	/* CONFIG_SMP */
#endif	/* __ASM_MCS_LOCK_H */
