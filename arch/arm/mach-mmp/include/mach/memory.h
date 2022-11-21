/*
 * linux/arch/arm/mach-mmp/include/mach/memory.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_MEMORY_H
#define __ASM_MACH_MEMORY_H

#if defined(CONFIG_CRASH_DUMP)
#define PLAT_PHYS_OFFSET	UL(0x06000000)
#elif defined(CONFIG_TZ_HYPERVISOR)
#define PLAT_PHYS_OFFSET	UL(0x00200000)
#else
#define PLAT_PHYS_OFFSET	UL(0x00000000)
#endif

/*
 * Restrict DMA-able region to workaround silicon bug.  The bug
 * restricts buffers available for DMA to AXI fabric 2 to be
 * below 64M
 */
#ifdef CONFIG_ZONE_DMA
#define ARM_DMA_ZONE_SIZE      SZ_64M
#endif

#endif /* __ASM_MACH_MEMORY_H */
