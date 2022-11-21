/*
 * linux/arch/arm/mach-mmp/ca9-hotplug.c
 *
 * Author:      Hong Feng <hongfeng@marvell.com>
 * Copyright:   (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/completion.h>
#include <linux/jiffies.h>

#include <asm/cacheflush.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <mach/regs-apmu.h>
#include <mach/pxa988_lowpower.h>
#include <mach/cputype.h>


static inline int apmu_core_status_zx(void)
{
	return cpu_is_pxa988_z1() || cpu_is_pxa986_z1() ||
		cpu_is_pxa988_z2() || cpu_is_pxa986_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z3();
}

static int pxa988_powergate_is_powered(u32 cpu)
{
	if (apmu_core_status_zx())
		return !(__raw_readl(APMU_CORE_STATUS) & (1 << (3 + 2 * cpu)));
	else
		return !(__raw_readl(APMU_CORE_STATUS) & (1 << (4 + 3 * cpu)));
}

/*
 * called from the request cpu,
 * make sure the target cpu is actully down after return
 */
int platform_cpu_kill(unsigned int cpu)
{
	int count = 10000;
	while (count--) {
		if (!pxa988_powergate_is_powered(cpu)) {
			pr_info("CPU%u: real shutdown\n", cpu);
			return 1;
		}

		udelay(10);
	}

	return 0;
}

/*
 * platform-specific code to shutdown a CPU,
 * Called with IRQs disabled
 */
void platform_cpu_die(unsigned int cpu)
{
	/* we're ready for shutdown now, so do it */
	pxa988_hotplug_enter(cpu, PXA988_LPM_D2_UDR);
	/* Never get here, Reset from pxa988_secondary_startup */
	panic("core reset should never get here");
}

int platform_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	return cpu == 0 ? -EPERM : 0;
}
