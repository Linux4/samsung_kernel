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
#include <linux/cputype.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/mach-types.h>
#include <asm/mcpm.h>

#include "common.h"
#include "reset.h"
#include "regs-addr.h"

#define APMU_WAKEUP_CORE(n)		(1 << (n & 0x3))
#define APMU_COREn_WAKEUP_CTL(n)	(0x012c + 4 * (n & 0x3))

static inline unsigned int get_core_count(void)
{
	u32 ret = 1;

	/* Read L2 control register */
	asm volatile("mrc p15, 1, %0, c9, c0, 2" : "=r"(ret));
	/* core count : [25:24] of L2 register + 1 */
	ret = ((ret >> 24) & 3) + 1;

	return ret;
}

void pxa988_gic_raise_softirq(const struct cpumask *mask, unsigned int irq)
{
	u32 val = 0;
	int targ_cpu;
	void __iomem *apmu_base;

	gic_raise_softirq(mask, irq);

	/* We shouldn't access any registers when AXI time out occurred */
	if (keep_silent)
		return;

	apmu_base = regs_addr_get_va(REGS_ADDR_APMU);
	BUG_ON(!apmu_base);
	/*
	 * Set the wakeup bits to make sure the core(s) can respond to
	 * the IPI interrupt.
	 * If the target core(s) is alive, this operation is ignored by
	 * the APMU. After the core wakes up, these corresponding bits
	 * are clearly automatically by PMU hardware.
	 */
	preempt_disable();
	for_each_cpu(targ_cpu, mask) {
		BUG_ON(targ_cpu >= CONFIG_NR_CPUS);
		val |= APMU_WAKEUP_CORE(targ_cpu);
	}
	__raw_writel(val, apmu_base +
				APMU_COREn_WAKEUP_CTL(smp_processor_id()));
	preempt_enable();
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
static void __init mmp_smp_init_cpus(void)
{
	unsigned int i, ncores = get_core_count();

	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

	set_smp_cross_call(pxa988_gic_raise_softirq);
}

bool __init mmp_smp_init_ops(void)
{
	mmp_smp_init_cpus();
	mcpm_smp_set_ops();

	return true;
}
