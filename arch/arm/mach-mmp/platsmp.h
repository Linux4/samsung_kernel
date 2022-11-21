/*
 * linux/arch/arm/mach-mmp/platsmp.h
 *
 * Author:	Neil Zhang <zhangwm@marvell.com>
 * Copyright:	(C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __PLATSMP_H__
#define __PLATSMP_H__

extern void pxa_cpu_reset(u32 cpu);
extern void __init pxa_cpu_reset_handler_init(void);
extern void __cpuinit pxa_secondary_init(u32 cpu);

#endif /* __PLATSMP_H__ */
