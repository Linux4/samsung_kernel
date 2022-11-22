/*
 *  linux/arch/arm/mach-mmp/irq-eden-icu.c
 *
 *  IRQ handling for EDEN SoC using ICU Only or GIC+ICU Configuration.
 *  Based on arch/arm/mach-mmp/irq-mmp3.c <kvedere@marvell.com>
 *  Copyright:	Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <mach/smp.h>
#include <mach/regs-icu.h>
#include <mach/irqs.h>
#include "common.h"

#define NUM_CA7_CORES	2
#define CA7_CORE_1	0
#define CA7_CORE_2	1

#ifndef CONFIG_SMP
static int core_id; /* Core ID for Non-SMP boot */
#else
static DEFINE_SPINLOCK(irq_affinity_lock);
#endif

static void __iomem *icu_int_sel_num[NUM_CA7_CORES] = {
	EDEN_ICU_IRQ1_SEL_INT_NUM,
	EDEN_ICU_IRQ2_SEL_INT_NUM
};

static uint32_t icu_int_route[NUM_CA7_CORES] = {
	ICU_INT_ROUTE_CA7_1_IRQ,
	ICU_INT_ROUTE_CA7_2_IRQ
};

#ifdef CONFIG_SMP
static void icu_mask_irq_smp(int irq)
{
	uint32_t _irq = irq - IRQ_EDEN_START, r, i;
	for (i = 0; i < NUM_CA7_CORES; i++) {
		r = __raw_readl(ICU_INT_CONF(_irq));
		r &= ~icu_int_route[i];
		__raw_writel(r, ICU_INT_CONF(_irq));
	}
}

static void ca7_mask_irq(int irq, int core_id)
{
	uint32_t _irq = irq - IRQ_EDEN_START;
	uint32_t r = __raw_readl(ICU_INT_CONF(_irq));
	r &= ~icu_int_route[core_id];
	__raw_writel(r, ICU_INT_CONF(_irq));
}

static void ca7_unmask_irq(int irq, int core_id)
{
	uint32_t _irq = irq - IRQ_EDEN_START;
	uint32_t r = __raw_readl(ICU_INT_CONF(_irq));
	r |= icu_int_route[core_id];
	__raw_writel(r, ICU_INT_CONF(_irq));
}

static int eden_irq_set_affinity(struct irq_data *d, const struct cpumask *dest, bool force)
{
	unsigned int cpu = cpumask_first(dest);
	unsigned int irq = d->irq;

	spin_lock(&irq_affinity_lock);

	switch (cpu) {
	case CA7_CORE_1:
		printk(KERN_INFO "%s: Unmask IRQ %d for Core 1/Mask it for Core 2\n", __func__, irq);
		ca7_mask_irq(irq, CA7_CORE_2);
		ca7_unmask_irq(irq, CA7_CORE_1);
		d->node = cpu;
		break;
	case CA7_CORE_2:
		printk(KERN_INFO "%s: Unmask IRQ %d for Core 2/Mask it for Core 1\n", __func__, irq);
		ca7_mask_irq(irq, CA7_CORE_1);
		ca7_unmask_irq(irq, CA7_CORE_2);
		d->node = cpu;
		break;
	default:
		BUG();
		break;
	}
	spin_unlock(&irq_affinity_lock);

	return 0;
}
#endif

static void icu_mask_irq(struct irq_data *d)
{
	uint32_t _irq = d->irq - IRQ_EDEN_START, r;
#ifdef CONFIG_SMP
	uint32_t core_id;

	spin_lock(&irq_affinity_lock);
	core_id = d->node;
#endif
	r = __raw_readl(ICU_INT_CONF(_irq));
	r &= ~icu_int_route[core_id];
	__raw_writel(r, ICU_INT_CONF(_irq));

#ifdef CONFIG_SMP
	spin_unlock(&irq_affinity_lock);
#endif
}

static void icu_unmask_irq(struct irq_data *d)
{
	uint32_t _irq = d->irq - IRQ_EDEN_START, r;
#ifdef CONFIG_SMP
	uint32_t core_id;

	spin_lock(&irq_affinity_lock);
	core_id = d->node;
#endif
	r = __raw_readl(ICU_INT_CONF(_irq));
	r |= icu_int_route[core_id];
	__raw_writel(r, ICU_INT_CONF(_irq));

#ifdef CONFIG_SMP
	spin_unlock(&irq_affinity_lock);
#endif
}

static struct irq_chip icu_irq_chip = {
	.name		= "icu_irq",
	.irq_mask	= icu_mask_irq,
	.irq_mask_ack	= icu_mask_irq,
	.irq_unmask	= icu_unmask_irq,
	.irq_disable	= icu_mask_irq,
#if defined(CONFIG_SMP)
	.irq_set_affinity   = eden_irq_set_affinity,
#endif
};

#define SECOND_IRQ_MASK(_name_, irq_base, prefix)			\
static void _name_##_mask_irq(struct irq_data *d)			\
{									\
	uint32_t r;							\
	r = __raw_readl(prefix##_MASK) | (1 << (d->irq - irq_base));	\
	__raw_writel(r, prefix##_MASK);					\
}

#define SECOND_IRQ_UNMASK(_name_, irq_base, prefix)			\
static void _name_##_unmask_irq(struct irq_data *d)			\
{									\
	uint32_t r;							\
	r = __raw_readl(prefix##_MASK) & ~(1 << (d->irq - irq_base));	\
	__raw_writel(r, prefix##_MASK);					\
}

#define SECOND_IRQ_DEMUX(_name_, irq_base, prefix)			\
static void _name_##_irq_demux(unsigned int irq, struct irq_desc *desc)	\
{									\
	unsigned long status, mask, n;					\
	mask = __raw_readl(prefix##_MASK);				\
	while (1) {							\
		status = __raw_readl(prefix##_STATUS) & ~mask;		\
		if (status == 0)					\
			break;						\
		n = find_first_bit(&status, BITS_PER_LONG);		\
		while (n < BITS_PER_LONG) {				\
			generic_handle_irq(irq_base + n);		\
			n = find_next_bit(&status, BITS_PER_LONG, n+1);	\
		}							\
	}								\
}

#define SECOND_IRQ_CHIP(_name_, irq_base, prefix)			\
SECOND_IRQ_MASK(_name_, irq_base, prefix)				\
SECOND_IRQ_UNMASK(_name_, irq_base, prefix)				\
SECOND_IRQ_DEMUX(_name_, irq_base, prefix)				\
static struct irq_chip _name_##_irq_chip = {				\
	.name		= #_name_,					\
	.irq_mask	= _name_##_mask_irq,				\
	.irq_unmask	= _name_##_unmask_irq,				\
	.irq_disable	= _name_##_mask_irq,				\
}

SECOND_IRQ_CHIP(pmic, IRQ_EDEN_INT4_BASE, EDEN_ICU_INT_4);
SECOND_IRQ_CHIP(rtc,  IRQ_EDEN_INT5_BASE,  EDEN_ICU_INT_5);
SECOND_IRQ_CHIP(gpu,  IRQ_EDEN_INT8_BASE,  EDEN_ICU_INT_8);
SECOND_IRQ_CHIP(twsi, IRQ_EDEN_INT17_BASE, EDEN_ICU_INT_17);
SECOND_IRQ_CHIP(misc1, IRQ_EDEN_INT18_BASE, EDEN_ICU_INT_18);
SECOND_IRQ_CHIP(misc2, IRQ_EDEN_INT30_BASE, EDEN_ICU_INT_30);
SECOND_IRQ_CHIP(misc3, IRQ_EDEN_INT35_BASE, EDEN_ICU_INT_35);
SECOND_IRQ_CHIP(ccic, IRQ_EDEN_INT42_BASE, EDEN_ICU_INT_42);
SECOND_IRQ_CHIP(ssp,  IRQ_EDEN_INT51_BASE,  EDEN_ICU_INT_51);
SECOND_IRQ_CHIP(misc4,  IRQ_EDEN_INT57_BASE,  EDEN_ICU_INT_57);

static void init_mux_irq(struct irq_chip *chip, int start, int num)
{
	int irq;

	for (irq = start; num > 0; irq++, num--) {
		struct irq_data *d = irq_get_irq_data(irq);

		/* mask and clear the IRQ */
		chip->irq_mask(d);
		if (chip->irq_ack)
			chip->irq_ack(d);

		irq_set_chip(irq, chip);
		set_irq_flags(irq, IRQF_VALID);
		irq_set_handler(irq, handle_level_irq);
	}
}

#ifdef CONFIG_SMP /* GIC+ICU Path */
static void icu_handle_irq(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_get_chip(irq);
	struct irq_data *d = irq_get_irq_data(irq);
	uint32_t core_id = hard_smp_processor_id();
	uint32_t r = __raw_readl(icu_int_sel_num[core_id]);

	if (r & (1<<6))
		generic_handle_irq((r & 0x3f) + IRQ_EDEN_START);

	/* primary controller end of interrupt */
	chip->irq_eoi(d);
	/* primary controller unmasking */
	chip->irq_unmask(d);
}
#endif

void __init eden_init_icu_gic(void)
{
	int irq;
	struct irq_chip *chip;

	/* Disable Interrupts to CA7 Cores */
	__raw_writel(0x1, EDEN_ICU_GBL_IRQ1_MSK);
	__raw_writel(0x1, EDEN_ICU_GBL_IRQ2_MSK);

#ifdef CONFIG_SMP
	spin_lock_init(&irq_affinity_lock);
#else
	core_id = hard_smp_processor_id();
#endif

	for (irq = IRQ_EDEN_START; irq < (IRQ_EDEN_START + 64); irq++) {
#ifdef CONFIG_SMP
		/* Mask IRQ to all CA7 cores */
		icu_mask_irq_smp(irq);
#else
		icu_mask_irq(irq_get_irq_data(irq));
#endif
		irq_set_chip(irq, &icu_irq_chip);
		set_irq_flags(irq, IRQF_VALID);

		switch (irq) {
		/* Muxed Interrupts */
		case IRQ_EDEN_INT4:
		case IRQ_EDEN_INT5:
		case IRQ_EDEN_INT8:
		case IRQ_EDEN_INT17:
		case IRQ_EDEN_INT18:
		case IRQ_EDEN_INT30:
		case IRQ_EDEN_INT35:
		case IRQ_EDEN_INT42:
		case IRQ_EDEN_INT51:
		case IRQ_EDEN_INT57:
			break;
		default:
			irq_set_handler(irq, handle_level_irq);
			break;
		}
	}

	init_mux_irq(&pmic_irq_chip, IRQ_EDEN_INT4_BASE, 4);
	init_mux_irq(&rtc_irq_chip, IRQ_EDEN_INT5_BASE, 2);
	init_mux_irq(&gpu_irq_chip, IRQ_EDEN_INT8_BASE, 4);
	init_mux_irq(&twsi_irq_chip, IRQ_EDEN_INT17_BASE, 6);
	init_mux_irq(&misc1_irq_chip, IRQ_EDEN_INT18_BASE, 3);
	init_mux_irq(&misc2_irq_chip, IRQ_EDEN_INT30_BASE, 3);
	init_mux_irq(&misc3_irq_chip, IRQ_EDEN_INT35_BASE, 32);
	init_mux_irq(&ccic_irq_chip, IRQ_EDEN_INT42_BASE, 2);
	init_mux_irq(&ssp_irq_chip, IRQ_EDEN_INT51_BASE, 2);
	init_mux_irq(&misc4_irq_chip, IRQ_EDEN_INT57_BASE, 24);

	irq_set_chained_handler(IRQ_EDEN_INT4, pmic_irq_demux);
	irq_set_chained_handler(IRQ_EDEN_INT5, rtc_irq_demux);
	irq_set_chained_handler(IRQ_EDEN_INT8, gpu_irq_demux);
	irq_set_chained_handler(IRQ_EDEN_INT17, twsi_irq_demux);
	irq_set_chained_handler(IRQ_EDEN_INT18, misc1_irq_demux);
	irq_set_chained_handler(IRQ_EDEN_INT30, misc2_irq_demux);
	irq_set_chained_handler(IRQ_EDEN_INT35, misc3_irq_demux);
	irq_set_chained_handler(IRQ_EDEN_INT42, ccic_irq_demux);
	irq_set_chained_handler(IRQ_EDEN_INT51, ssp_irq_demux);
	irq_set_chained_handler(IRQ_EDEN_INT57, misc4_irq_demux);

#ifdef CONFIG_SMP /* GIC+ICU Path */
	irq_set_chained_handler(IRQ_LEGACYIRQ, icu_handle_irq);
	chip = irq_get_chip(IRQ_LEGACYIRQ);
	chip->irq_unmask(irq_get_irq_data(IRQ_LEGACYIRQ));
#endif
	/* Enable Interrupts to CA7 Cores */
	__raw_writel(0x0, EDEN_ICU_GBL_IRQ1_MSK);
	__raw_writel(0x0, EDEN_ICU_GBL_IRQ2_MSK);
}
