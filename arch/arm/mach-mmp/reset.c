/*
 * linux/arch/arm/mach-mmp/reset.c
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
#include <asm/cacheflush.h>
#include <asm/mach/map.h>
#include <asm/mcpm.h>

#include "reset.h"
#include "regs-addr.h"

#define PMU_CC2_AP		0x0100
#define PXA1088_CPU_POR_RST(n)	(1 << ((n) * 3 + 16))
#define PXA1088_CPU_CORE_RST(n)	(1 << ((n) * 3 + 17))
#define PXA1088_CPU_DBG_RST(n)	(1 << ((n) * 3 + 18))

#define CIU_WARM_RESET_VECTOR	0x00d8
/*
 * This function is called from boot_secondary to bootup the secondary cpu.
 */
void mmp_cpu_power_up(unsigned int cpu, unsigned int cluster)
{
	u32 tmp;
	void __iomem *apmu_base;

	BUG_ON(cpu == 0);

	apmu_base = regs_addr_get_va(REGS_ADDR_APMU);
	BUG_ON(!apmu_base);

	tmp = readl(apmu_base + PMU_CC2_AP);
	if (tmp & PXA1088_CPU_CORE_RST(cpu)) {
		/* Release secondary core from reset */
		tmp &= ~(PXA1088_CPU_POR_RST(cpu)
			| PXA1088_CPU_CORE_RST(cpu) | PXA1088_CPU_DBG_RST(cpu));
		writel(tmp, apmu_base + PMU_CC2_AP);
	}
}

void __init mmp_entry_vector_init(void)
{
	void __iomem *ciu_base;

	ciu_base = regs_addr_get_va(REGS_ADDR_CIU);
	BUG_ON(!ciu_base);

	writel(__pa(mcpm_entry_point), ciu_base + CIU_WARM_RESET_VECTOR);
}
