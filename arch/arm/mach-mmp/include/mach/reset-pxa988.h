/*
 * linux/arch/arm/mach-mmp/include/mach/reset-pxa988.h
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __RESET_PXA988_H__
#define __RESET_PXA988_H__

#ifdef CONFIG_CPU_PXA988
#define CPU_CORE_RST(n)	(1 << ((n) * 4 + 16))
#define CPU_DBG_RST(n)	(1 << ((n) * 4 + 18))
#define CPU_WDOG_RST(n)	(1 << ((n) * 4 + 19))
#elif defined(CONFIG_CPU_PXA1088)
#define CPU_POR_RST(n)	(1 << ((n) * 3 + 16))
#define CPU_CORE_RST(n)	(1 << ((n) * 3 + 17))
#define CPU_DBG_RST(n)	(1 << ((n) * 3 + 18))
#endif

extern u32 pm_reserve_pa;
extern u32 reset_handler_pa;
extern u32 secondary_cpu_handler;
extern void pxa988_secondary_startup(void);
extern void pxa988_hotplug_handler(void);
extern void pxa988_return_handler(void);
extern void pxa988_set_reset_handler(u32 fn, u32 cpu);
extern void pxa988_cpu_reset_entry(void);
extern void pxa_cpu_reset(u32 cpu);
extern void pxa988_gic_raise_softirq(const struct cpumask *mask,
	unsigned int irq);

#ifdef CONFIG_PM
extern u32 l2sram_shutdown;
#ifdef CONFIG_CACHE_L2X0
extern u32 l2x0_regs_phys;
extern u32 l2x0_saved_regs_phys_addr;
#endif
extern void pxa988_cpu_resume_handler(void);
#endif

extern void __init pxa_cpu_reset_handler_init(void);

#endif /* __RESET_PXA988_H__ */
