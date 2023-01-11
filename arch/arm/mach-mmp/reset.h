/*
 * linux/arch/arm/mach-mmp/include/mach/reset.h
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

void mmp_cpu_power_up(unsigned int cpu, unsigned int cluster);
void mmp_set_entry_vector(u32 cpu, u32 addr);
void __init mmp_entry_vector_init(void);

extern void gic_raise_softirq(const struct cpumask *mask, unsigned int irq);

#endif /* __RESET_PXA988_H__ */
