/*
 *  linux/arch/arm/mach-mmp/irq-eden.c
 *
 *  Generic IRQ handling, GPIO IRQ demultiplexing, etc.
 *  Based on arch/arm/mach-mmp/irq-mmp3.c <kvedere@marvell.com>
 *  Copyright:	Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <mach/regs-icu.h>

static void icu_mask_irq(int irq)
{
	uint32_t r = __raw_readl(ICU_INT_CONF(irq));

	r &= ~(ICU_INT_ROUTE_CA7_1_FIQ | ICU_INT_ROUTE_CA7_1_IRQ |
		ICU_INT_ROUTE_CA7_2_FIQ | ICU_INT_ROUTE_CA7_2_IRQ);
	__raw_writel(r, ICU_INT_CONF(irq));
}

void __init eden_init_gic(void)
{
	int i;

	/* disable global irq of ICU for CA7-1, CA7-2 */
	__raw_writel(0x1, EDEN_ICU_GBL_FIQ1_MSK);
	__raw_writel(0x1, EDEN_ICU_GBL_IRQ1_MSK);
	__raw_writel(0x1, EDEN_ICU_GBL_FIQ2_MSK);
	__raw_writel(0x1, EDEN_ICU_GBL_IRQ2_MSK);

	for (i = 0; i < 64; i++)
		icu_mask_irq(i);
}
