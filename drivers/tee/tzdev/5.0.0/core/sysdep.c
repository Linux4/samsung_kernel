/*
 * Copyright (C) 2015 Samsung Electronics, Inc.
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

#include <linux/compiler.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cred.h>
#include <linux/crypto.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#if defined(CONFIG_SOC_S5E9925) || defined(CONFIG_SOC_S5E8825)
#include <soc/samsung/exynos-acme.h>
#endif

#include "core/sysdep.h"

static __maybe_unused unsigned int gup_flags(int write, int force)
{
	unsigned int flags = 0;

	if (write)
		flags |= FOLL_WRITE;
	if (force)
		flags |= FOLL_FORCE;

	return flags;
}

void sysdep_register_cpu_notifier(struct notifier_block* notifier,
		int (*startup)(unsigned int cpu),
		int (*teardown)(unsigned int cpu))
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "tee/teegris:online",
			startup, teardown);
#else
	/* Register PM notifier */
	register_cpu_notifier(notifier);
#endif
}

void sysdep_unregister_cpu_notifier(struct notifier_block* notifier)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	cpuhp_remove_state_nocalls(CPUHP_AP_ONLINE_DYN);
#else
	unregister_cpu_notifier(notifier);
#endif
}

void sysdep_get_ts(struct tz_ree_time *ree_ts)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
	struct timespec64 ts;
	ktime_get_real_ts64(&ts);
#else
	struct timespec ts;
	getnstimeofday(&ts);
#endif
	ree_ts->sec = ts.tv_sec;
	ree_ts->nsec = ts.tv_nsec;
}

void sysdep_shash_desc_init(struct shash_desc *desc, struct crypto_shash *tfm)
{
		desc->tfm = tfm;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
		desc->flags = 0;
#endif
}

int sysdep_pid_refcount_read(struct pid *pid)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
	return atomic_read(&pid->count);
#else
	return refcount_read(&pid->count);
#endif
}

int sysdep_cpufreq_register_notifier(struct notifier_block *nb, unsigned int list)
{
#if defined(CONFIG_SOC_S5E9925) || defined(CONFIG_SOC_S5E8825)
	return exynos_cpufreq_register_notifier(nb, list);
#elif defined(CONFIG_SOC_S5E9935)
	// FIXME: change to alternative function (cpufreq_register_notifier() returns -EBUSY)
	cpufreq_register_notifier(nb, list);
	return 0;
#else
	return cpufreq_register_notifier(nb, list);
#endif
}

void sysdep_pinned_vm_add(struct mm_struct *mm, unsigned long nr_pages)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0)
	mm->pinned_vm += nr_pages;
#else
	atomic64_add(nr_pages, &mm->pinned_vm);
#endif
}

void sysdep_pinned_vm_sub(struct mm_struct *mm, unsigned long nr_pages)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0)
	mm->pinned_vm -= nr_pages;
#else
	atomic64_sub(nr_pages, &mm->pinned_vm);
#endif
}

struct file *sysdep_get_exe_file(struct task_struct *task)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	struct mm_struct *mm;
	struct file *exe_file = NULL;

	mm = get_task_mm(task);
	if (!mm)
		return NULL;

	rcu_read_lock();
	exe_file = rcu_dereference(mm->exe_file);
	if (exe_file && !get_file_rcu(exe_file))
		exe_file = NULL;
	rcu_read_unlock();
	mmput(mm);

	return exe_file;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 5)
	return get_task_exe_file(task);
#else
	return get_mm_exe_file(task->mm);
#endif
}
