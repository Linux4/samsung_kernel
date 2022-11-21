/*
 * linux/arch/arm/mach-mmp/wakeupgen.c
 *
 * This mmp wakeup generation is the interrupt controller extension used
 * along with ARM GIC to wake the CPU out from low power states on external
 * interrupts. This extension always keeps the mask status and wake up
 * affinity the same mapping as in the GIC. In the normal CPU active mode,
 * the global interrupts in the icu are masked, external interrupts route
 * directly to the GIC.

 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/irq.h>
#include <linux/io.h>
#include <asm/hardware/gic.h>
#include <mach/cputype.h>
#include <mach/irqs.h>
#include <mach/regs-icu.h>
#include <mach/regs-apbc.h>

#define ICU_INT_CONF_CPU1	(1 << 7)
#define ICU_INT_CONF_CPU0	(1 << 6)
#define ICU_INT_CONF_AP(n)	(1 << (6 + (n & 1)))
#define ICU_INT_CONF_AP_MASK	(3 << 6)
#define ICU_INT_CONF_IRQ_FIQ	(1 << 4)
#define ICU_INT_CONF_PRIO(n)	(n & 0xF)
#define ICU_INT_CONF_MASKING	(0 << 0)

#define ICU_IRQ_CPU0_MASKED	(ICU_INT_CONF_IRQ_FIQ | ICU_INT_CONF_CPU0)

/*
 * Return:
 * >=0: the irq line for ICU
 * -1: no need to handle this irq line
 */
static inline int irq_need_handle(struct irq_data *d)
{
	/* Avoid modify CP interrupts */
	if (d->irq == IRQ_PXA988_AIRQ
	 || d->irq == IRQ_PXA988_CP_TIMER1
	 || d->irq == IRQ_PXA988_CP_TIMER2_3)
		return -1;

	/* out of the ICU range */
	if (d->irq >= IRQ_PXA988_END)
		return -1;

#ifdef CONFIG_CPU_PXA988
	/* WORKAROUND: Trigger IPC interrupt to wake cores when sending IPI */
	if (d->irq == IRQ_PXA988_IPC_AP3)
		return -1;
#endif

	/*
	 * the icu connects the sys_int_ap interrupt source to the SPI interface
	 * of the GIC. since the GIC SPI interrupts start from 32, here minus 32
	 * from the GIC spi irq number to get the raw icu based irq.
	 * For PPI and SGI, just skip it.
	 */
	return d->irq - IRQ_PXA988_START;
}

static void icu_mask_irq(struct irq_data *d)
{
	void __iomem *reg;
	unsigned long val;
	int irq;

	irq = irq_need_handle(d);
	if (irq < 0)
		return;

	reg = ICU_INT_CONF(irq);
	val = __raw_readl(reg);
	val &= ~0xF;
	__raw_writel(val, reg);
}

static void icu_unmask_irq(struct irq_data *d)
{
	void __iomem *reg;
	unsigned long val;
	int irq;

	irq = irq_need_handle(d);
	if (irq < 0)
		return;

	reg = ICU_INT_CONF(irq);
	val = __raw_readl(reg);
	val &= ~0xF;
	val |= ICU_INT_CONF_PRIO(1);
	__raw_writel(val, reg);
}

static int icu_set_affinity(struct irq_data *d,
	const struct cpumask *mask_val, bool force)
{
	void __iomem *reg;
	unsigned long val;
	unsigned int cpu;
	int irq;

	irq = irq_need_handle(d);
	if (irq < 0)
		return 0;

	cpu = cpumask_first(mask_val);
	reg = ICU_INT_CONF(irq);
	val = __raw_readl(reg);
	val &= ~ICU_INT_CONF_AP_MASK;
	val |= ICU_INT_CONF_AP(cpu);
	__raw_writel(val, reg);

	return 0;
}

void __init mmp_wakeupgen_init(void)
{
	int irq;

	/* disable global irq/fiq in icu for all the cores */
#ifdef CONFIG_CPU_PXA988
	__raw_writel(0x3, PXA988_ICU_CP_GBL_INT_MSK);
	__raw_writel(0x3, PXA988_ICU_A9C0_GBL_INT_MSK);
	__raw_writel(0x3, PXA988_ICU_A9C1_GBL_INT_MSK);
#endif
	/*
	 * config all the interrupt source be able to interrupt the cpu 0,
	 * except those which are routed to Seagull
	 * in IRQ mode, with priority 0 as masked by default.
	 */
	for (irq = 0; irq < 64; irq++) {
		if (irq == IRQ_PXA988_AIRQ - 32
		 || irq == IRQ_PXA988_CP_TIMER1 - 32
		 || irq == IRQ_PXA988_CP_TIMER2_3 - 32)
			__raw_writel(__raw_readl(ICU_INT_CONF(irq)) &
					~ICU_INT_CONF_CPU0, ICU_INT_CONF(irq));
		else
			__raw_writel(ICU_IRQ_CPU0_MASKED, ICU_INT_CONF(irq));
	}

#ifdef CONFIG_CPU_PXA988
	if (cpu_pxa98x_stepping() < PXA98X_A0) {
		/*
		* WORKAROUND: "Trigger IPC interrupt to wake cores when sending
		* IPI"
		* Enable the IPC AP3 in ICU to let the IPC interrupt be able to
		* wake up two AP cores.
		*/
		__raw_writel(APBC_APBCLK | APBC_RST, APBC_PXA988_IPC);
		__raw_writel(APBC_APBCLK | APBC_FNCLK, APBC_PXA988_IPC);

	#define ICU_IRQ_CPU_ALL_ENABLE	(ICU_INT_CONF_IRQ_FIQ |\
		ICU_INT_CONF_CPU0 | ICU_INT_CONF_CPU1 | ICU_INT_CONF_PRIO(2))
		irq = IRQ_PXA988_IPC_AP3 - IRQ_PXA988_START;
		__raw_writel(ICU_IRQ_CPU_ALL_ENABLE, ICU_INT_CONF(irq));
	}
#endif

	gic_arch_extn.irq_mask = icu_mask_irq;
	gic_arch_extn.irq_unmask = icu_unmask_irq;
	gic_arch_extn.irq_set_affinity = icu_set_affinity;
}
