/*
 *  linux/arch/arm/mach-mmp/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <asm/cacheflush.h>
#include <mach/hardware.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/localtimer.h>
#include <asm/smp_scu.h>

#include <mach/irqs.h>
#include <mach/addr-map.h>
#include <mach/regs-apmu.h>
#include <mach/features.h>
#include <plat/pxa_trace.h>

#include "platsmp.h"

#ifdef CONFIG_TZ_HYPERVISOR
#include <asm/smp_plat.h>
#include "./tzlc/pxa_tzlc.h"
#endif

#if (defined CONFIG_DEBUG_FS) && ((defined CONFIG_CPU_PXA988)\
|| (defined CONFIG_CPU_PXA1088))
#include <mach/pxa988_lowpower.h>
#include <mach/clock-pxa988.h>
#endif

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
volatile int __cpuinitdata pen_release = -1;

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}
#ifdef CONFIG_HAVE_ARM_SCU
void notrace __iomem *pxa_scu_base_addr(void)
{
	return (void __iomem *)SCU_VIRT_BASE;
}

static inline unsigned int get_core_count(void)
{
	void __iomem *scu_base = pxa_scu_base_addr();
	if (scu_base)
		return scu_get_core_count(scu_base);
	return 1;
}
#else
static inline unsigned int get_core_count(void)
{
#ifdef CONFIG_CPU_CA7MP
	unsigned int val;
	/* Read L2 control register */
	asm volatile("mrc p15, 1, %0, c9, c0, 2" : "=r"(val));
	/* core count : [25:24] of L2 register + 1 */
	val = ((val>>24) & 3) + 1;
	return val;
#else
	return 1;
#endif
}
#endif

static DEFINE_SPINLOCK(boot_lock);

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	trace_pxa_core_hotplug(HOTPLUG_EXIT, cpu);
#if (defined CONFIG_DEBUG_FS) && ((defined CONFIG_CPU_PXA988)\
|| (defined CONFIG_CPU_PXA1088))
	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_EXIT, PXA988_MAX_LPM_INDEX);
#endif

	pxa_secondary_init(cpu);

	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	gic_secondary_init(0);

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	/*
	 * Avoid timer calibration on slave cpus. Use the value calibrated
	 * on master cpu. Referenced from tegra3
	 */
	preset_lpj = loops_per_jiffy;

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */

	spin_lock(&boot_lock);
	/*
	 * The secondary processor is waiting to be released from
	 * the holding pen - release it, then wait for it to flag
	 * that it has been released by resetting pen_release.
	 *
	 * Note that "pen_release" is the hardware CPU ID, whereas
	 * "cpu" is Linux's internal ID.
	 */
	write_pen_release(cpu);

	/* reset the cpu, let it branch to the kernel entry */
	pxa_cpu_reset(cpu);

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return pen_release != -1 ? -ENOSYS : 0;
}

#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
void pxa988_gic_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	unsigned int val = 0;
	int targ_cpu;

#ifdef CONFIG_TZ_HYPERVISOR
	int cpu;
	unsigned long map = 0;
	tzlc_cmd_desc cmd_desc;
	tzlc_handle tzlc_handle;

	/* Convert our logical CPU mask into a physical one. */
	for_each_cpu(cpu, mask)
		map |= 1 << cpu_logical_map(cpu);

	/*
	 * Ensure that stores to Normal memory are visible to the
	 * other CPUs before issuing the IPI.
	 */
	dsb();

	tzlc_handle = pxa_tzlc_create_handle();

	cmd_desc.op = TZLC_CMD_TRIGER_SGI;
	cmd_desc.args[0] = map << 16 | irq;
	pxa_tzlc_cmd_op(tzlc_handle, &cmd_desc);

	pxa_tzlc_destroy_handle(tzlc_handle);
#else
	gic_raise_softirq(mask, irq);
#endif

	if (has_feat_ipc_wakeup_core()) {
		#define IPCC_VIRT_BASE	(APB_VIRT_BASE + 0x1D800)
		/*
		* "Trigger IPC interrupt to wake cores when sending IPI"
		* Trigger IPC GP_INT to generate IRQ19 as wake up source inside
		* the ICU to wake up the cores when sending IPI.
		*/
		__raw_writel(0x400, IPCC_VIRT_BASE + 0x8); /* IPC_ISRW */
	} else {
		/*
		 * Set the wakeup bits to make sure the core(s) can respond to
		 * the IPI interrupt.
		 * If the target core(s) IS alive, this operation is ignored by
		 * the APMU. After the core wakes up, these corresponding bits
		 * are clearly automatically by PMU hardware.
		 */
		for_each_cpu(targ_cpu, mask) {
			BUG_ON(targ_cpu >= CONFIG_NR_CPUS);
			val |= APMU_WAKEUP_CORE(targ_cpu);
		}
		__raw_writel(val, APMU_COREn_WAKEUP_CTL(smp_processor_id()));
	}
}
#endif

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
	unsigned int i, ncores = get_core_count();

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
	set_smp_cross_call(pxa988_gic_raise_softirq);
#else
	set_smp_cross_call(gic_raise_softirq);
#endif
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int i;

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);
#ifdef CONFIG_HAVE_ARM_SCU
	scu_enable(pxa_scu_base_addr());
#endif

	pxa_cpu_reset_handler_init();
}
