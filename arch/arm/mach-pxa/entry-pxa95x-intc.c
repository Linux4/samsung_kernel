/*
*  arch/arm/mach-pxa/entry-pxa95x-intc.c
*
*  IRQ handler for PXA95x-based platforms
*
*  Copyright (C) 2012 Marvell International Ltd.
*
*  This program is free software; you can redistribute it and/or  modify
*  it under the terms of the GNU General Public License version 2 as
*  publishhed by the Free Software Foundation.
*/

#include <mach/regs-intc.h>
#include <asm/irq.h>
#include <linux/io.h>

/* bit31 of ICHP - valid IRQ */
#define ICHP_VALID_IRQ   0x80000000
/* bit22:16 of ICHP - IRQ Highest Priority Field  */
#define ICHP_IRQ_OFFSET  16
#define ICHP_IRQ_MASK    0x7F


void pxa95x_handle_irq_intc(struct pt_regs * regs)
{
	u32 irqnr;

	while (ICHP & ICHP_VALID_IRQ) {
		/* IRQ Highest Priority Field */
		irqnr = (ICHP >> ICHP_IRQ_OFFSET) & ICHP_IRQ_MASK;
		asm_do_IRQ(irqnr, regs);
	}
}

