/*
 * linux/arch/arm/mach-mmp/reset-pxa988.c
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
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/hardware/gic.h>
#include <asm/cacheflush.h>
#include <asm/mach/map.h>

#include <mach/regs-ciu.h>
#include <mach/regs-apmu.h>
#include <mach/reset-pxa988.h>
#include <mach/regs-coresight.h>
#include <mach/pxa988_lowpower.h>

#ifdef CONFIG_SMP
static u32 *reset_handler;

/*
 * This function is called from boot_secondary to bootup the secondary cpu.
 * It maybe called when system bootup or add a plugged cpu into system.
 *
 * cpu here can only be 1 since we only have two cores.
 */
void pxa_cpu_reset(u32 cpu)
{
	u32 tmp;

	BUG_ON(cpu != 1);

	tmp = readl(PMU_CC2_AP);
	if (tmp & CPU1_CORE_RST) {
		/* Core 1 first bootup, we need to release core from reset */
		tmp &= ~(CPU1_CORE_RST | CPU1_DBG_RST | CPU1_WDOG_RST);
		writel(tmp, PMU_CC2_AP);
	} else {
		pxa988_gic_raise_softirq(cpumask_of(cpu), 1);
		check_and_swrst_core1();
	}
}

/* This function is called from platform_secondary_init in platform.c */
void __cpuinit pxa_secondary_init(u32 cpu)
{
#ifdef CONFIG_CORESIGHT_SUPPORT
	static int bootup = 1;

	/* restore the coresight registers when hotplug in */
	if (!bootup)
		v7_coresight_restore();
	else
		bootup = 0;
#endif

	core1_c2 = 1;

#ifdef CONFIG_PM
	/* Use resume handler as the default handler when hotplugin */
	pxa988_set_reset_handler(__pa(pxa988_cpu_resume_handler), cpu);
#endif
}

void pxa988_set_reset_handler(u32 fn, u32 cpu)
{
	reset_handler[cpu] = fn;
}


/*
 * Check if core1 successfully exits from c2.
 * If not, send a software reset to it.
 * If it's still in c2, panic
 */
void check_and_swrst_core1(void)
{
	u32 timeout;
	u32 pmu_cc2_ap;
	bool reset = 0;

check:
	timeout = 3000;
	while(timeout-- && !core1_c2)
		udelay(1);

	if (timeout == 0) {
		if (reset)
			panic("Even software reset can't wakeup core1!\n");
		else {
			printk(KERN_WARNING "CPU1 can't be woken up!\n");
			pmu_cc2_ap = __raw_readl(PMU_CC2_AP);
			__raw_writel(pmu_cc2_ap | CPU1_CORE_RST, PMU_CC2_AP);
			udelay(1);
			__raw_writel(pmu_cc2_ap & ~CPU1_CORE_RST, PMU_CC2_AP);
			reset = 1;
			goto check;
		}
	}
}


void __init pxa_cpu_reset_handler_init(void)
{
	int cpu;

	/* Assign the address for saving reset handler */
	reset_handler_pa = pm_reserve_pa + PAGE_SIZE;
	reset_handler = (u32 *)__arm_ioremap(reset_handler_pa,
						PAGE_SIZE, MT_MEMORY_SO);
	if (reset_handler == NULL)
		panic("failed to remap memory for reset handler!\n");
	memset(reset_handler, 0x0, PAGE_SIZE);

	/* Flush the addr to DDR */
	__cpuc_flush_dcache_area((void *)&reset_handler_pa,
				sizeof(reset_handler_pa));
	outer_clean_range(__pa(&reset_handler_pa),
		__pa(&reset_handler_pa + sizeof(reset_handler_pa)));

	/* We will reset from DDR directly by default */
	writel(__pa(pxa988_cpu_reset_entry), CIU_CA9_WARM_RESET_VECTOR);

#ifdef CONFIG_PM
	/* Setup the resume handler for the first core */
	pxa988_set_reset_handler(__pa(pxa988_cpu_resume_handler), 0);
#endif

	/* Setup the handler for secondary cores */
	for (cpu = 1; cpu < CONFIG_NR_CPUS; cpu++)
		pxa988_set_reset_handler(__pa(pxa988_secondary_startup), cpu);

#ifdef CONFIG_HOTPLUG_CPU
	/* Setup the handler for Hotplug cores */
	writel(__pa(pxa988_secondary_startup), &secondary_cpu_handler);
	__cpuc_flush_dcache_area((void *)&secondary_cpu_handler,
				sizeof(secondary_cpu_handler));
	outer_clean_range(__pa(&secondary_cpu_handler),
		__pa(&secondary_cpu_handler + sizeof(secondary_cpu_handler)));
#endif
}

#elif defined(CONFIG_PM)

/* Set resume handler directly for UP if CONFIG_PM is enabled */
void __init pxa_cpu_reset_handler_init(void)
{
	writel(__pa(pxa988_cpu_resume_handler), CIU_CA9_WARM_RESET_VECTOR);
}
#endif
