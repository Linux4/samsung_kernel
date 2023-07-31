/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
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

#ifndef __SYSDEP_H__
#define __SYSDEP_H__

#include <linux/completion.h>
#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/version.h>

#include <asm/barrier.h>
#include <asm/cacheflush.h>

#include <crypto/hash.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#include <uapi/linux/sched/types.h>
#include <linux/wait.h>

#ifndef CPU_UP_CANCELED
#define CPU_UP_CANCELED		0x0004
#endif
#ifndef CPU_DOWN_PREPARE
#define CPU_DOWN_PREPARE	0x0005
#endif

typedef struct wait_queue_entry wait_queue_t;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/refcount.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
#include <crypto/hash.h>
#endif

/* For MAX_RT_PRIO definitions */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
#include <linux/sched/prio.h>
#else
#include <linux/sched/rt.h>
#endif

#include "core/ree_time.h"

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 13, 0)
#define U8_MAX		((u8)~0U)
#define S8_MAX		((s8)(U8_MAX>>1))
#define S8_MIN		((s8)(-S8_MAX - 1))
#define U16_MAX		((u16)~0U)
#define S16_MAX		((s16)(U16_MAX>>1))
#define S16_MIN		((s16)(-S16_MAX - 1))
#define U32_MAX		((u32)~0U)
#define S32_MAX		((s32)(U32_MAX>>1))
#define S32_MIN		((s32)(-S32_MAX - 1))
#define U64_MAX		((u64)~0ULL)
#define S64_MAX		((s64)(U64_MAX>>1))
#define S64_MIN		((s64)(-S64_MAX - 1))
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define for_each_cpu_mask(cpu, mask)	for_each_cpu((cpu), &(mask))
#define cpu_isset(cpu, mask)		cpumask_test_cpu((cpu), &(mask))
#else
#define cpumask_pr_args(maskp)		nr_cpu_ids, cpumask_bits(maskp)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
static inline void shash_desc_zero(struct shash_desc *desc)
{
	memset(desc, 0,
		sizeof(*desc) + crypto_shash_descsize(desc->tfm));
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
#define SHASH_DESC_ON_STACK(shash, ctx)                           \
	char __##shash##_desc[sizeof(struct shash_desc) +         \
		crypto_shash_descsize(ctx)] CRYPTO_MINALIGN_ATTR; \
	struct shash_desc *shash = (struct shash_desc *)__##shash##_desc
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
static inline int atomic_fetch_or(int mask, atomic_t *v)
{
	int c, old;

	old = atomic_read(v);
	do {
		c = old;
		old = atomic_cmpxchg((v), c, c | mask);
	} while (old != c);

	return old;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
static inline int atomic_fetch_and(int mask, atomic_t *v)
{
	int c, old;

	old = atomic_read(v);
	do {
		c = old;
		old = atomic_cmpxchg((v), c, c & mask);
	} while (old != c);

	return old;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
static inline void reinit_completion(struct completion *x)
{
	x->done = 0;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0)
#define set_mb(var, value)		\
	do {				\
		var = value;		\
		smp_mb();		\
	} while (0)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
static inline int check_mem_region(unsigned long start, unsigned long n)
{
	(void) start;
	(void) n;

	return 0;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
#define VERIFY_READ 0
#define VERIFY_WRITE 1
#define sysdep_access_ok(type, addr, size)	access_ok(addr, size)
#else
#define sysdep_access_ok(type, addr, size)	access_ok(type, addr, size)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#define sysdep_alloc_workqueue_attrs()		alloc_workqueue_attrs()
#else
#define sysdep_alloc_workqueue_attrs()		alloc_workqueue_attrs(GFP_KERNEL)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#define sysdep_mm_down_read(mm)			down_read(&mm->mmap_lock)
#define sysdep_mm_up_read(mm)			up_read(&mm->mmap_lock)
#define sysdep_mm_down_write(mm)		down_write(&mm->mmap_lock)
#define sysdep_mm_up_write(mm)			up_write(&mm->mmap_lock)
#else
#define sysdep_mm_down_read(mm)			down_read(&mm->mmap_sem)
#define sysdep_mm_up_read(mm)			up_read(&mm->mmap_sem)
#define sysdep_mm_down_write(mm)		down_write(&mm->mmap_sem)
#define sysdep_mm_up_write(mm)			up_write(&mm->mmap_sem)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#define sysdep_get_online_cpus()			cpus_read_lock()
#define sysdep_put_online_cpus()			cpus_read_unlock()
#else
#define sysdep_get_online_cpus()			get_online_cpus()
#define sysdep_put_online_cpus()			put_online_cpus()
#endif

void sysdep_get_ts(struct tz_ree_time *ree_ts);
void sysdep_register_cpu_notifier(struct notifier_block *notifier,
		int (*startup)(unsigned int cpu),
		int (*teardown)(unsigned int cpu));
void sysdep_unregister_cpu_notifier(struct notifier_block *notifier);
int sysdep_crypto_file_sha1(uint8_t *hash, struct file *file);
void sysdep_shash_desc_init(struct shash_desc *desc, struct crypto_shash *tfm);
int sysdep_pid_refcount_read(struct pid *pid);
int sysdep_cpufreq_register_notifier(struct notifier_block *nb, unsigned int list);
void sysdep_pinned_vm_add(struct mm_struct *mm, unsigned long nr_pages);
void sysdep_pinned_vm_sub(struct mm_struct *mm, unsigned long nr_pages);
struct file *sysdep_get_exe_file(struct task_struct *task);

#endif /* __SYSDEP_H__ */
