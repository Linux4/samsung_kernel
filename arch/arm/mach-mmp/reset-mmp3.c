/*
 * linux/arch/arm/mach-mmp/reset-mmp3.c
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/smp.h>

#include <asm/hardware/gic.h>
#include <asm/unified.h>
#include <asm/io.h>

#include <mach/addr-map.h>


#define SW_BRANCH_VIRT_ADDR	(AXI_VIRT_BASE + 0x82c24)

extern void mmp3_secondary_startup(void);

void pxa_cpu_reset(u32 cpu)
{
	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 */
	gic_raise_softirq(cpumask_of(cpu), 1);
}

void __cpuinit pxa_secondary_init(u32 cpu)
{
	return;
}

void __init pxa_cpu_reset_handler_init(void)
{
	/*
	 * Write the address of secondary startup into the system-wide
	 * flags register. The BootMonitor waits for this register to
	 * become non-zero.
	 */
	__raw_writel(virt_to_phys(mmp3_secondary_startup),
			(void __iomem *)SW_BRANCH_VIRT_ADDR);
}
