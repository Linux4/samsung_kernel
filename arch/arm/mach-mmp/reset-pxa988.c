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

#include <asm/io.h>
#include <asm/hardware/gic.h>
#include <asm/cacheflush.h>
#include <asm/mach/map.h>

#include <mach/regs-ciu.h>
#include <mach/regs-apmu.h>
#include <mach/reset-pxa988.h>
#ifdef CONFIG_DEBUG_FS
#include <mach/pxa988_lowpower.h>
#include <mach/clock-pxa988.h>
#endif

#ifdef CONFIG_TZ_HYPERVISOR
#include "./tzlc/pxa_tzlc.h"
#endif

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

	/*
	 * only need to confirm it's not core0, since the caller will validate
	 * cpu id.
	 */
	BUG_ON(cpu == 0);

#ifdef CONFIG_ARM_ERRATA_802022
	pxa988_set_reset_handler(__pa(pxa988_hotplug_handler), cpu);
#endif

	tmp = readl(PMU_CC2_AP);

	if ((tmp & CPU_CORE_RST(cpu)) == CPU_CORE_RST(cpu)) {
		/* secondary core bootup, we need to release it from reset */
#ifdef CONFIG_CPU_PXA988
		tmp &= ~(CPU_CORE_RST(cpu)
			| CPU_DBG_RST(cpu) | CPU_WDOG_RST(cpu));
#elif defined(CONFIG_CPU_PXA1088)
		tmp &= ~(CPU_CORE_RST(cpu)
			| CPU_DBG_RST(cpu) | CPU_POR_RST(cpu));
#endif
		writel(tmp, PMU_CC2_AP);
	} else {
#ifdef CONFIG_ARM_ERRATA_802022
		pxa988_set_reset_handler(__pa(pxa988_hotplug_handler), cpu);
#endif
		pxa988_gic_raise_softirq(cpumask_of(cpu), 1);
	}
}

/* This function is called from platform_secondary_init in platform.c */
void __cpuinit pxa_secondary_init(u32 cpu)
{
#ifdef CONFIG_DEBUG_FS
	pxa988_cpu_dcstat_event(cpu, CPU_IDLE_EXIT, PXA988_MAX_LPM_INDEX);
#endif

#ifdef CONFIG_PM
	/* Use resume handler as the default handler when hotplugin */
	pxa988_set_reset_handler(__pa(pxa988_cpu_resume_handler), cpu);
#endif
}

void pxa988_set_reset_handler(u32 fn, u32 cpu)
{
	reset_handler[cpu] = fn;
}

void __init pxa_cpu_reset_handler_init(void)
{
	int cpu;
#ifdef CONFIG_TZ_HYPERVISOR
	tzlc_cmd_desc cmd_desc;
	tzlc_handle tzlc_hdl;
#endif

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
		__pa(&reset_handler_pa + 1));

/*
 * with TrustZone enabled, CIU_WARM_RESET_VECTOR is used by TrustZone software,
 * and kernel use CIU_SW_SCRATCH_REG to save the cpu reset entry.
*/
#ifdef CONFIG_TZ_HYPERVISOR
	tzlc_hdl = pxa_tzlc_create_handle();
	cmd_desc.op = TZLC_CMD_SET_WARM_RESET_ENTRY;
	cmd_desc.args[0] = __pa(pxa988_cpu_reset_entry);
	pxa_tzlc_cmd_op(tzlc_hdl, &cmd_desc);
	pxa_tzlc_destroy_handle(tzlc_hdl);
#else
	/* We will reset from DDR directly by default */
	writel(__pa(pxa988_cpu_reset_entry), CIU_WARM_RESET_VECTOR);
#endif

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
		__pa(&secondary_cpu_handler + 1));
#endif
}
