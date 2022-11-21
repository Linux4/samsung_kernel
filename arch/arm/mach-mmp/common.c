/*
 *  linux/arch/arm/mach-mmp/common.c
 *
 *  Code common to PXA168 processor lines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/page.h>
#include <asm/mach/map.h>
#include <asm/system_misc.h>
#include <mach/addr-map.h>
#include <mach/cputype.h>

#include "common.h"

#define MMP_CHIPID	(AXI_VIRT_BASE + 0x82c00)

unsigned int mmp_chip_id;
EXPORT_SYMBOL(mmp_chip_id);

static struct map_desc standard_io_desc[] __initdata = {
	{
		.pfn		= __phys_to_pfn(APB_PHYS_BASE),
		.virtual	= (unsigned long)APB_VIRT_BASE,
		.length		= APB_PHYS_SIZE,
		.type		= MT_DEVICE,
	}, {
		.pfn		= __phys_to_pfn(AXI_PHYS_BASE),
		.virtual	= (unsigned long)AXI_VIRT_BASE,
		.length		= AXI_PHYS_SIZE,
		.type		= MT_DEVICE,
#ifdef PERI_PHYS_BASE
	}, {
		.pfn		= __phys_to_pfn(PERI_PHYS_BASE),
		.virtual	= (unsigned long)PERI_VIRT_BASE,
		.length		= PERI_PHYS_SIZE,
		.type		= MT_DEVICE,
#endif /* PERI_PHYS_BASE */
#ifdef DMCU_PHYS_BASE
	}, {
		.pfn		= __phys_to_pfn(DMCU_PHYS_BASE),
		.virtual	= DMCU_VIRT_BASE,
		.length		= DMCU_PHYS_SIZE,
		.type		= MT_DEVICE,
#endif
#ifdef CONFIG_CPU_MMP3
	}, {
		.pfn		= __phys_to_pfn(AUD_PHYS_BASE),
		.virtual	= (unsigned long)AUD_VIRT_BASE,
		.length		= AUD_PHYS_SIZE,
		.type		= MT_DEVICE,
	}, {
		.pfn		= __phys_to_pfn(AUD_PHYS_BASE2),
		.virtual	= (unsigned long)AUD_VIRT_BASE2,
		.length		= AUD_PHYS_SIZE2,
		.type		= MT_DEVICE,
#endif
	},
};

void __init mmp_map_io(void)
{
	iotable_init(standard_io_desc, ARRAY_SIZE(standard_io_desc));

	/* this is early, initialize mmp_chip_id here */
	mmp_chip_id = __raw_readl(MMP_CHIPID);
}

void mmp_restart(char mode, const char *cmd)
{
	soft_restart(0);
}
