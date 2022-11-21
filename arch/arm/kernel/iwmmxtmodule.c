/*
 * linux/arch/arm/kernel/iwmmxtmodule.c
 *
 * PJ4 iWMMXt coprocessor context switching and handling
 *
 * Copyright (c) 2010 Marvell International Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/smp.h>
#include <asm/thread_notify.h>

struct iwmmxt_struct *last_iwmmxt_context[NR_CPUS];

extern void iwmmxt_save_state(struct iwmmxt_struct *storage);

static u32 iwmmxt_cp_access_read(void)
{
	u32 value;
#if defined(CONFIG_CPU_PJ4) || defined(CONFIG_CPU_PJ4B)
	__asm__ __volatile__ (
		"mrc	p15, 0, %0, c1, c0, 2\n\t"
		: "=r" (value));
#else
	__asm__ __volatile__ (
		"mrc	p15, 0, %0, c15, c1, 0\n\t"
		: "=r" (value));
#endif
	return value;
}

static void iwmmxt_cp_access_write(u32 value)
{
	u32 temp;

#if defined(CONFIG_CPU_PJ4) || defined(CONFIG_CPU_PJ4B)
	__asm__ __volatile__ (
		"mcr	p15, 0, %1, c1, c0, 2\n\t"
		"mrc	p15, 0, %0, c1, c0, 2\n\t"
		"mov	%0, %0\n\t"
		"sub	pc, pc, #4\n\t"
		: "=r" (temp) : "r" (value));
#else
	__asm__ __volatile__ (
		"mcr	p15, 0, %1, c15, c1, 0\n\t"
		"mrc	p15, 0, %0, c15, c1, 0\n\t"
		"mov	%0, %0\n\t"
		"sub	pc, pc, #4\n\t"
		: "=r" (temp) : "r" (value));
#endif
}

static u32 iwmmxt_enable_cp_access(void)
{
	u32 value, temp;

	value = temp = iwmmxt_cp_access_read();
#if defined(CONFIG_CPU_PJ4) || defined(CONFIG_CPU_PJ4B)
	temp |= 0xf;
#else
	temp |= 0x3;
#endif
	iwmmxt_cp_access_write(temp);

	return value;
}

static u32 iwmmxt_disable_cp_access(void)
{
	u32 value, temp;

	value = temp = iwmmxt_cp_access_read();
#if defined(CONFIG_CPU_PJ4) || defined(CONFIG_CPU_PJ4B)
	temp &= ~0xf;
#else
	temp &= ~0x3;
#endif
	iwmmxt_cp_access_write(temp);

	return value;
}

#ifdef CONFIG_SMP
static int iwmmxt_is_cp_accessible(void)
{
	u32 value, mask;

	value = iwmmxt_cp_access_read();
#if defined(CONFIG_CPU_PJ4) || defined(CONFIG_CPU_PJ4B)
	mask = 0xf;
#else
	mask = 0x3;
#endif

	return value & mask;
}
#endif

static inline void dsp_save_state(u32 *state)
{
	__asm__ __volatile__ (
		"mrrc	p0, 0, %0, %1, c0\n"
		: "=r" (state[0]), "=r" (state[1]));
}

static inline void dsp_load_state(u32 *state)
{
	__asm__ __volatile__ (
		"mcrr	p0, 0, %0, %1, c0\n"
		: : "r" (state[0]), "r" (state[1]));
}

static int dsp_do(struct notifier_block *self, unsigned long cmd, void *t)
{
	struct thread_info *thread = t;

	switch (cmd) {
	case THREAD_NOTIFY_FLUSH:
		thread->cpu_context.extra[0] = 0;
		thread->cpu_context.extra[1] = 0;
		break;

	case THREAD_NOTIFY_SWITCH:
		dsp_save_state(current_thread_info()->cpu_context.extra);
		dsp_load_state(thread->cpu_context.extra);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block dsp_notifier_block = {
	.notifier_call	= dsp_do,
};

static int __init cpu_has_iwmmxt(void)
{
#if defined(CONFIG_CPU_PJ4) || defined(CONFIG_CPU_PJ4B)
	return 1;
#else
	u32 lo;
	u32 hi;
	u32 cp_access;

	cp_access = iwmmxt_enable_cp_access();
	/*
	 * This sequence is interpreted by the DSP coprocessor as:
	 *	mar	acc0, %2, %3
	 *	mra	%0, %1, acc0
	 *
	 * And by the iWMMXt coprocessor as:
	 *	tmcrr	wR0, %2, %3
	 *	tmrrc	%0, %1, wR0
	 */
	__asm__ __volatile__ (
		"mcrr	p0, 0, %2, %3, c0\n"
		"mrrc	p0, 0, %0, %1, c0\n"
		: "=r" (lo), "=r" (hi)
		: "r" (0), "r" (0x100));

	iwmmxt_cp_access_write(cp_access);
	return !!hi;
#endif
}

void iwmmxt_sync_hwstate(struct thread_info *thread)
{
	unsigned int cpu = get_cpu();

	/*
	 * If the thread we're interested in is the current owner of the
	 * hardware IWMMX state, then we need to save its state.
	 */
	if (last_iwmmxt_context[cpu] == &thread->fpstate.iwmmxt) {
		u32 value = iwmmxt_enable_cp_access();

		iwmmxt_save_state(&thread->fpstate.iwmmxt);
		iwmmxt_cp_access_write(value);
	}

	put_cpu();
}

void iwmmxt_flush_hwstate(struct thread_info *thread)
{
	unsigned int cpu = get_cpu();

	/*
	 * If the thread we're interested in is the current owner of the
	 * hardware VFP state, then we need to make it reloaded.
	 */
	if (last_iwmmxt_context[cpu] == &thread->fpstate.iwmmxt) {
		iwmmxt_disable_cp_access();
		/*
		 * Set the context to NULL to force a reload the next time
		 * the thread uses the IWMMX.
		 */
		last_iwmmxt_context[cpu] = NULL;
	}

#ifdef CONFIG_SMP
	/*
	 * For SMP we still have to take care of the case where the thread
	 * migrates to another CPU and then back to the original CPU on which
	 * the last IWMMX user is still the same thread. Mark the thread IWMMX
	 * state as belonging to a non-existent CPU so that the saved one will
	 * be reloaded in the above case.
	 */
	thread->fpstate.iwmmxt.cpu = NR_CPUS;
#endif
	put_cpu();
}

static int iwmmxt_notifier(struct notifier_block *self, unsigned long cmd, void *t)
{
	struct thread_info *thread = t;
	unsigned int cpu = thread->cpu;

	switch (cmd) {
	case THREAD_NOTIFY_SWITCH:
#ifdef CONFIG_SMP

		/*
		 * On SMP, if IWMMX is enabled, save the old state in
		 * case the thread migrates to a different CPU. The
		 * restoring is done lazily.
		 */
		if (iwmmxt_is_cp_accessible() && last_iwmmxt_context[cpu]) {
			iwmmxt_save_state(last_iwmmxt_context[cpu]);
			last_iwmmxt_context[cpu]->cpu = cpu;
		}
		/*
		 * Thread migration, just force the reloading of the
		 * state on the new CPU in case the IWMMX registers
		 * contain stale data.
		 */
		if (thread->fpstate.iwmmxt.cpu != cpu)
			last_iwmmxt_context[cpu] = NULL;

#endif
		iwmmxt_disable_cp_access();
		break;
	case THREAD_NOTIFY_FLUSH:
		/*
		 * flush_thread() zeroes thread->fpstate, so no need
		 * to do anything here.
		 *
		 * FALLTHROUGH: Ensure we don't try to overwrite our newly
		 * initialised state information on the first fault.
		 */

	case THREAD_NOTIFY_EXIT:
		if (last_iwmmxt_context[cpu] == &thread->fpstate.iwmmxt)
			last_iwmmxt_context[cpu] = NULL;
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block iwmmxt_notifier_block = {
	.notifier_call	= iwmmxt_notifier,
};

/*
 * Disable CP0/CP1 on boot, and let call_fpe() and the iWMMXt lazy
 * switch code handle iWMMXt context switching.
 */
static int __init iwmmxt_init(void)
{
	iwmmxt_disable_cp_access();

	if (cpu_has_iwmmxt()) {
		printk(KERN_INFO "PJ4 iWMMXt coprocessor enabled.\n");
		elf_hwcap |= HWCAP_IWMMXT;
		thread_register_notifier(&iwmmxt_notifier_block);
	}
	else {
		u32 cp_access;

		printk(KERN_INFO "XScale DSP coprocessor detected.\n");
		cp_access = iwmmxt_cp_access_read() & ~3;
		iwmmxt_cp_access_write(cp_access | 1);
		thread_register_notifier(&dsp_notifier_block);
	}

	return 0;
}

late_initcall(iwmmxt_init);
