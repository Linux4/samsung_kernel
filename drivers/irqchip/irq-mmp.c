/*
 *  linux/arch/arm/mach-mmp/irq.c
 *
 *  Generic IRQ handling, GPIO IRQ demultiplexing, etc.
 *  Copyright (C) 2008 - 2012 Marvell Technology Group Ltd.
 *
 *  Author:	Bin Yang <bin.yang@marvell.com>
 *              Haojian Zhuang <haojian.zhuang@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/irqchip/mmp.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>

#include <asm/exception.h>
#include <asm/hardirq.h>

#include "irqchip.h"

#define MAX_ICU_NR		16

#define PJ1_INT_SEL		0x10c
#define PJ4_INT_SEL		0x104
#define PXA1928_INT_SEL		0x108

/* bit fields in PJ1_INT_SEL and PJ4_INT_SEL */
#define SEL_INT_PENDING		(1 << 6)
#define SEL_INT_NUM_MASK	0x3f

#define WAKE_CLR_OFFSET		0x7c
struct icu_wake_clr {
	int	irq;
	int	mask;
};

struct icu_chip_data {
	int			nr_irqs;
	unsigned int		virq_base;
	unsigned int		cascade_irq;
	void __iomem		*reg_status;
	void __iomem		*reg_mask;
	unsigned int		conf_enable;
	unsigned int		conf_disable;
	unsigned int		conf_mask;
	unsigned int		clr_mfp_irq_base;
	unsigned int		clr_mfp_hwirq;
	struct irq_domain	*domain;
	struct	icu_wake_clr	*wake_clr;
	int			nr_wake_clr;
};

struct mmp_intc_conf {
	unsigned int	conf_enable;
	unsigned int	conf_disable;
	unsigned int	conf_mask;
};

static void __iomem *apmu_virt_addr;
static void __iomem *mmp_icu_base;
static struct icu_chip_data icu_data[MAX_ICU_NR];
static int max_icu_nr;
static u32 irq_for_cp[64];
static u32 irq_for_cp_nr;	/* How many irqs will be routed to cp */
static u32 irq_for_sp[32];
static u32 irq_for_sp_nr;	/* How many irqs will be routed to sp */

extern void mmp2_clear_pmic_int(void);

static int irq_ignore_wakeup(struct icu_chip_data *data, int hwirq)
{
	int i;

	if (hwirq < 0 || hwirq >= data->nr_irqs)
		return 1;

	for (i = 0; i < irq_for_cp_nr; i++)
		if (irq_for_cp[i] == hwirq)
			return 1;

	return 0;
}

static void icu_mask_irq_wakeup(struct irq_data *d)
{
	struct icu_chip_data *data = &icu_data[0];
	int hwirq = d->hwirq - data->virq_base;
	u32 r;

	if (irq_ignore_wakeup(data, hwirq))
		return;

	r = readl_relaxed(mmp_icu_base + (hwirq << 2));
	r &= ~data->conf_mask;
	r |= data->conf_disable;
	writel_relaxed(r, mmp_icu_base + (hwirq << 2));
}

static void icu_unmask_irq_wakeup(struct irq_data *d)
{
	struct icu_chip_data *data = &icu_data[0];
	int hwirq = d->irq - data->virq_base;
	u32 r;

	if (irq_ignore_wakeup(data, hwirq))
		return;

	r = readl_relaxed(mmp_icu_base + (hwirq << 2));
	r &= ~data->conf_mask;
	r |= data->conf_enable;
	writel_relaxed(r, mmp_icu_base + (hwirq << 2));
}

static int icu_set_affinity(struct irq_data *d,
	const struct cpumask *mask_val, bool force)
{
	struct icu_chip_data *data = &icu_data[0];
	struct irq_desc *desc = container_of(d, struct irq_desc, irq_data);
	int hwirq = d->irq - data->virq_base;
	u32 r, cpu;

	if (irq_ignore_wakeup(data, hwirq))
		return 0;

	r = readl_relaxed(mmp_icu_base + (hwirq << 2));
	r &= ~ICU_INT_CONF_AP_MASK;

	if (unlikely(desc->action && desc->action->flags & IRQF_MULTI_CPUS)) {
		for_each_cpu_and(cpu, mask_val, cpu_online_mask) {
			if (cpu <= 3)
				r |= ICU_INT_CONF_AP(cpu);
			else if (cpu <= 7)
				r |= ICU_INT_CONF_AP2(cpu);
			else {
				pr_err("Wrong CPU number: %d\n", cpu);
				BUG_ON(1);
			}
		}
	} else {
		cpu = cpumask_first(mask_val);
		if (cpu <= 3)
			r |= ICU_INT_CONF_AP(cpu);
		else if (cpu <= 7)
			r |= ICU_INT_CONF_AP2(cpu);
		else {
			pr_err("Wrong CPU number: %d\n", cpu);
			BUG_ON(1);
		}
	}

	writel_relaxed(r, mmp_icu_base + (hwirq << 2));

	return 0;
}

void __iomem *icu_get_base_addr(void)
{
	return mmp_icu_base;
}

static void icu_mask_ack_irq(struct irq_data *d)
{
	struct irq_domain *domain = d->domain;
	struct icu_chip_data *data = (struct icu_chip_data *)domain->host_data;
	int hwirq;
	u32 r;

	hwirq = d->irq - data->virq_base;
	if (data == &icu_data[0]) {
		r = readl_relaxed(mmp_icu_base + (hwirq << 2));
		r &= ~data->conf_mask;
		r |= data->conf_disable;
		writel_relaxed(r, mmp_icu_base + (hwirq << 2));
	} else {
#ifdef CONFIG_CPU_MMP2
		if ((data->virq_base == data->clr_mfp_irq_base)
			&& (hwirq == data->clr_mfp_hwirq))
			mmp2_clear_pmic_int();
#endif
		r = readl_relaxed(data->reg_mask) | (1 << hwirq);
		writel_relaxed(r, data->reg_mask);
	}
}

static void icu_mask_irq(struct irq_data *d)
{
	struct irq_domain *domain = d->domain;
	struct icu_chip_data *data = (struct icu_chip_data *)domain->host_data;
	int hwirq;
	u32 r;

	hwirq = d->irq - data->virq_base;
	if (data == &icu_data[0]) {
		r = readl_relaxed(mmp_icu_base + (hwirq << 2));
		r &= ~data->conf_mask;
		r |= data->conf_disable;
		writel_relaxed(r, mmp_icu_base + (hwirq << 2));
	} else {
		r = readl_relaxed(data->reg_mask) | (1 << hwirq);
		writel_relaxed(r, data->reg_mask);
	}
}

static void icu_unmask_irq(struct irq_data *d)
{
	struct irq_domain *domain = d->domain;
	struct icu_chip_data *data = (struct icu_chip_data *)domain->host_data;
	int hwirq;
	u32 r;

	hwirq = d->irq - data->virq_base;
	if (data == &icu_data[0]) {
		r = readl_relaxed(mmp_icu_base + (hwirq << 2));
		r &= ~data->conf_mask;
		r |= data->conf_enable;
		writel_relaxed(r, mmp_icu_base + (hwirq << 2));
	} else {
		r = readl_relaxed(data->reg_mask) & ~(1 << hwirq);
		writel_relaxed(r, data->reg_mask);
	}
}

static void icu_eoi_clr_wakeup(struct irq_data *d)
{
	struct icu_chip_data *data = &icu_data[0];
	int hwirq = d->hwirq - data->virq_base;
	u32 val;
	int i;

	for (i = 0; i < data->nr_wake_clr; i++) {
		if (hwirq == data->wake_clr[i].irq) {
			val = readl_relaxed(apmu_virt_addr + WAKE_CLR_OFFSET);
			writel_relaxed(val | data->wake_clr[i].mask, \
				apmu_virt_addr + WAKE_CLR_OFFSET);
			break;
		}
	}
	return;
}

struct irq_chip icu_irq_chip = {
	.name		= "icu_irq",
	.irq_mask	= icu_mask_irq,
	.irq_mask_ack	= icu_mask_ack_irq,
	.irq_unmask	= icu_unmask_irq,
};

static void icu_mux_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	struct irq_domain *domain;
	struct icu_chip_data *data;
	int i;
	unsigned long mask, status, n;

	for (i = 1; i < max_icu_nr; i++) {
		if (irq == icu_data[i].cascade_irq) {
			domain = icu_data[i].domain;
			data = (struct icu_chip_data *)domain->host_data;
			break;
		}
	}
	if (i >= max_icu_nr) {
		pr_err("Spurious irq %d in MMP INTC\n", irq);
		return;
	}

	mask = readl_relaxed(data->reg_mask);
	while (1) {
		status = readl_relaxed(data->reg_status) & ~mask;
		if (status == 0)
			break;
		for_each_set_bit(n, &status, BITS_PER_LONG) {
			generic_handle_irq(icu_data[i].virq_base + n);
		}
	}
}

static int mmp_irq_domain_map(struct irq_domain *d, unsigned int irq,
			      irq_hw_number_t hw)
{
	irq_set_chip_and_handler(irq, &icu_irq_chip, handle_level_irq);
	set_irq_flags(irq, IRQF_VALID);
	return 0;
}

static int mmp_irq_domain_xlate(struct irq_domain *d, struct device_node *node,
				const u32 *intspec, unsigned int intsize,
				unsigned long *out_hwirq,
				unsigned int *out_type)
{
	*out_hwirq = intspec[0];
	return 0;
}

const struct irq_domain_ops mmp_irq_domain_ops = {
	.map		= mmp_irq_domain_map,
	.xlate		= mmp_irq_domain_xlate,
};

static struct mmp_intc_conf mmp_conf = {
	.conf_enable	= 0x51,
	.conf_disable	= 0x0,
	.conf_mask	= 0x7fff,
};

static struct mmp_intc_conf mmp2_conf = {
	.conf_enable	= 0x20,
	.conf_disable	= 0x0,
	.conf_mask	= 0x7f,
};

static asmlinkage void __exception_irq_entry
mmp_handle_irq(struct pt_regs *regs)
{
	int irq, hwirq;

	hwirq = readl_relaxed(mmp_icu_base + PJ1_INT_SEL);
	if (!(hwirq & SEL_INT_PENDING))
		return;
	hwirq &= SEL_INT_NUM_MASK;
	irq = irq_find_mapping(icu_data[0].domain, hwirq);
	handle_IRQ(irq, regs);
}

static asmlinkage void __exception_irq_entry
mmp2_handle_irq(struct pt_regs *regs)
{
	int irq, hwirq;

	hwirq = readl_relaxed(mmp_icu_base + PJ4_INT_SEL);
	if (!(hwirq & SEL_INT_PENDING))
		return;
	hwirq &= SEL_INT_NUM_MASK;
	irq = irq_find_mapping(icu_data[0].domain, hwirq);
	handle_IRQ(irq, regs);
}

/* MMP (ARMv5) */
void __init icu_init_irq(void)
{
	int irq;

	max_icu_nr = 1;
	mmp_icu_base = ioremap(0xd4282000, 0x1000);
	icu_data[0].conf_enable = mmp_conf.conf_enable;
	icu_data[0].conf_disable = mmp_conf.conf_disable;
	icu_data[0].conf_mask = mmp_conf.conf_mask;
	icu_data[0].nr_irqs = 64;
	icu_data[0].virq_base = 0;
	icu_data[0].domain = irq_domain_add_legacy(NULL, 64, 0, 0,
						   &irq_domain_simple_ops,
						   &icu_data[0]);
	for (irq = 0; irq < 64; irq++) {
		icu_mask_irq(irq_get_irq_data(irq));
		irq_set_chip_and_handler(irq, &icu_irq_chip, handle_level_irq);
		set_irq_flags(irq, IRQF_VALID);
	}
	irq_set_default_host(icu_data[0].domain);
	set_handle_irq(mmp_handle_irq);
}

/* MMP2 (ARMv7) */
void __init mmp2_init_icu(void)
{
	int irq, end;

	max_icu_nr = 8;
	mmp_icu_base = ioremap(0xd4282000, 0x1000);
	icu_data[0].conf_enable = mmp2_conf.conf_enable;
	icu_data[0].conf_disable = mmp2_conf.conf_disable;
	icu_data[0].conf_mask = mmp2_conf.conf_mask;
	icu_data[0].nr_irqs = 64;
	icu_data[0].virq_base = 0;
	icu_data[0].domain = irq_domain_add_legacy(NULL, 64, 0, 0,
						   &irq_domain_simple_ops,
						   &icu_data[0]);
	icu_data[1].reg_status = mmp_icu_base + 0x150;
	icu_data[1].reg_mask = mmp_icu_base + 0x168;
	icu_data[1].clr_mfp_irq_base = icu_data[0].virq_base +
				icu_data[0].nr_irqs;
	icu_data[1].clr_mfp_hwirq = 1;		/* offset to IRQ_MMP2_PMIC_BASE */
	icu_data[1].nr_irqs = 2;
	icu_data[1].cascade_irq = 4;
	icu_data[1].virq_base = icu_data[0].virq_base + icu_data[0].nr_irqs;
	icu_data[1].domain = irq_domain_add_legacy(NULL, icu_data[1].nr_irqs,
						   icu_data[1].virq_base, 0,
						   &irq_domain_simple_ops,
						   &icu_data[1]);
	icu_data[2].reg_status = mmp_icu_base + 0x154;
	icu_data[2].reg_mask = mmp_icu_base + 0x16c;
	icu_data[2].nr_irqs = 2;
	icu_data[2].cascade_irq = 5;
	icu_data[2].virq_base = icu_data[1].virq_base + icu_data[1].nr_irqs;
	icu_data[2].domain = irq_domain_add_legacy(NULL, icu_data[2].nr_irqs,
						   icu_data[2].virq_base, 0,
						   &irq_domain_simple_ops,
						   &icu_data[2]);
	icu_data[3].reg_status = mmp_icu_base + 0x180;
	icu_data[3].reg_mask = mmp_icu_base + 0x17c;
	icu_data[3].nr_irqs = 3;
	icu_data[3].cascade_irq = 9;
	icu_data[3].virq_base = icu_data[2].virq_base + icu_data[2].nr_irqs;
	icu_data[3].domain = irq_domain_add_legacy(NULL, icu_data[3].nr_irqs,
						   icu_data[3].virq_base, 0,
						   &irq_domain_simple_ops,
						   &icu_data[3]);
	icu_data[4].reg_status = mmp_icu_base + 0x158;
	icu_data[4].reg_mask = mmp_icu_base + 0x170;
	icu_data[4].nr_irqs = 5;
	icu_data[4].cascade_irq = 17;
	icu_data[4].virq_base = icu_data[3].virq_base + icu_data[3].nr_irqs;
	icu_data[4].domain = irq_domain_add_legacy(NULL, icu_data[4].nr_irqs,
						   icu_data[4].virq_base, 0,
						   &irq_domain_simple_ops,
						   &icu_data[4]);
	icu_data[5].reg_status = mmp_icu_base + 0x15c;
	icu_data[5].reg_mask = mmp_icu_base + 0x174;
	icu_data[5].nr_irqs = 15;
	icu_data[5].cascade_irq = 35;
	icu_data[5].virq_base = icu_data[4].virq_base + icu_data[4].nr_irqs;
	icu_data[5].domain = irq_domain_add_legacy(NULL, icu_data[5].nr_irqs,
						   icu_data[5].virq_base, 0,
						   &irq_domain_simple_ops,
						   &icu_data[5]);
	icu_data[6].reg_status = mmp_icu_base + 0x160;
	icu_data[6].reg_mask = mmp_icu_base + 0x178;
	icu_data[6].nr_irqs = 2;
	icu_data[6].cascade_irq = 51;
	icu_data[6].virq_base = icu_data[5].virq_base + icu_data[5].nr_irqs;
	icu_data[6].domain = irq_domain_add_legacy(NULL, icu_data[6].nr_irqs,
						   icu_data[6].virq_base, 0,
						   &irq_domain_simple_ops,
						   &icu_data[6]);
	icu_data[7].reg_status = mmp_icu_base + 0x188;
	icu_data[7].reg_mask = mmp_icu_base + 0x184;
	icu_data[7].nr_irqs = 2;
	icu_data[7].cascade_irq = 55;
	icu_data[7].virq_base = icu_data[6].virq_base + icu_data[6].nr_irqs;
	icu_data[7].domain = irq_domain_add_legacy(NULL, icu_data[7].nr_irqs,
						   icu_data[7].virq_base, 0,
						   &irq_domain_simple_ops,
						   &icu_data[7]);
	end = icu_data[7].virq_base + icu_data[7].nr_irqs;
	for (irq = 0; irq < end; irq++) {
		icu_mask_irq(irq_get_irq_data(irq));
		if (irq == icu_data[1].cascade_irq ||
		    irq == icu_data[2].cascade_irq ||
		    irq == icu_data[3].cascade_irq ||
		    irq == icu_data[4].cascade_irq ||
		    irq == icu_data[5].cascade_irq ||
		    irq == icu_data[6].cascade_irq ||
		    irq == icu_data[7].cascade_irq) {
			irq_set_chip(irq, &icu_irq_chip);
			irq_set_chained_handler(irq, icu_mux_irq_demux);
		} else {
			irq_set_chip_and_handler(irq, &icu_irq_chip,
						 handle_level_irq);
		}
		set_irq_flags(irq, IRQF_VALID);
	}
	irq_set_default_host(icu_data[0].domain);
	set_handle_irq(mmp2_handle_irq);
}

#ifdef CONFIG_OF
static int __init mmp_init_bases(struct device_node *node)
{
	int ret, nr_irqs, irq, irq_base;

	ret = of_property_read_u32(node, "mrvl,intc-nr-irqs", &nr_irqs);
	if (ret) {
		pr_err("Not found mrvl,intc-nr-irqs property\n");
		return ret;
	}

	mmp_icu_base = of_iomap(node, 0);
	if (!mmp_icu_base) {
		pr_err("Failed to get interrupt controller register\n");
		return -ENOMEM;
	}

	irq_base = irq_alloc_descs(-1, 0, nr_irqs - NR_IRQS_LEGACY, 0);
	if (irq_base < 0) {
		pr_err("Failed to allocate IRQ numbers\n");
		ret = irq_base;
		goto err;
	} else if (irq_base != NR_IRQS_LEGACY) {
		pr_err("ICU's irqbase should be started from 0\n");
		ret = -EINVAL;
		goto err;
	}
	icu_data[0].nr_irqs = nr_irqs;
	icu_data[0].virq_base = 0;
	icu_data[0].domain = irq_domain_add_legacy(node, nr_irqs, 0, 0,
						   &mmp_irq_domain_ops,
						   &icu_data[0]);
	for (irq = 0; irq < nr_irqs; irq++)
		icu_mask_irq(irq_get_irq_data(irq));
	return 0;
err:
	iounmap(mmp_icu_base);
	return ret;
}

static int __init mmp_of_init(struct device_node *node,
			      struct device_node *parent)
{
	int ret;

	ret = mmp_init_bases(node);
	if (ret < 0)
		return ret;

	icu_data[0].conf_enable = mmp_conf.conf_enable;
	icu_data[0].conf_disable = mmp_conf.conf_disable;
	icu_data[0].conf_mask = mmp_conf.conf_mask;
	irq_set_default_host(icu_data[0].domain);
	set_handle_irq(mmp_handle_irq);
	max_icu_nr = 1;
	return 0;
}

static int __init mmp2_of_init(struct device_node *node,
			       struct device_node *parent)
{
	int ret;

	ret = mmp_init_bases(node);
	if (ret < 0)
		return ret;

	icu_data[0].conf_enable = mmp2_conf.conf_enable;
	icu_data[0].conf_disable = mmp2_conf.conf_disable;
	icu_data[0].conf_mask = mmp2_conf.conf_mask;
	irq_set_default_host(icu_data[0].domain);
	set_handle_irq(mmp2_handle_irq);
	max_icu_nr = 1;
	return 0;
}

static int __init mmp2_mux_of_init(struct device_node *node,
				   struct device_node *parent)
{
	struct resource res;
	int i, irq_base, ret, irq;
	u32 nr_irqs, mfp_irq;

	if (!parent)
		return -ENODEV;

	i = max_icu_nr;
	ret = of_property_read_u32(node, "mrvl,intc-nr-irqs",
				   &nr_irqs);
	if (ret) {
		pr_err("Not found mrvl,intc-nr-irqs property\n");
		return -EINVAL;
	}
	ret = of_address_to_resource(node, 0, &res);
	if (ret < 0) {
		pr_err("Not found reg property\n");
		return -EINVAL;
	}
	icu_data[i].reg_status = mmp_icu_base + res.start;
	ret = of_address_to_resource(node, 1, &res);
	if (ret < 0) {
		pr_err("Not found reg property\n");
		return -EINVAL;
	}
	icu_data[i].reg_mask = mmp_icu_base + res.start;
	icu_data[i].cascade_irq = irq_of_parse_and_map(node, 0);
	if (!icu_data[i].cascade_irq)
		return -EINVAL;

	irq_base = irq_alloc_descs(-1, 0, nr_irqs, 0);
	if (irq_base < 0) {
		pr_err("Failed to allocate IRQ numbers for mux intc\n");
		return irq_base;
	}
	if (!of_property_read_u32(node, "mrvl,clr-mfp-irq",
				  &mfp_irq)) {
		icu_data[i].clr_mfp_irq_base = irq_base;
		icu_data[i].clr_mfp_hwirq = mfp_irq;
	}
	irq_set_chained_handler(icu_data[i].cascade_irq,
				icu_mux_irq_demux);
	icu_data[i].nr_irqs = nr_irqs;
	icu_data[i].virq_base = irq_base;
	icu_data[i].domain = irq_domain_add_legacy(node, nr_irqs,
						   irq_base, 0,
						   &mmp_irq_domain_ops,
						   &icu_data[i]);
	for (irq = irq_base; irq < irq_base + nr_irqs; irq++)
		icu_mask_irq(irq_get_irq_data(irq));
	max_icu_nr++;
	return 0;
}
IRQCHIP_DECLARE(mmp_intc, "mrvl,mmp-intc", mmp_of_init);
IRQCHIP_DECLARE(mmp2_intc, "mrvl,mmp2-intc", mmp2_of_init);
IRQCHIP_DECLARE(mmp2_mux_intc, "mrvl,mmp2-mux-intc", mmp2_mux_of_init);

void __init mmp_of_wakeup_init(void)
{
	struct device_node *node;
	const __be32 *wake_clr_devs;
	int ret, nr_irqs;
	int size, i = 0, j = 0;
	int irq;

	node = of_find_compatible_node(NULL, NULL, "mrvl,mmp-intc-wakeupgen");
	if (!node) {
		pr_err("Failed to find interrupt controller in arch-mmp\n");
		return;
	}

	mmp_icu_base = of_iomap(node, 0);
	if (!mmp_icu_base) {
		pr_err("Failed to get interrupt controller register\n");
		return;
	}

	ret = of_property_read_u32(node, "mrvl,intc-nr-irqs", &nr_irqs);
	if (ret) {
		pr_err("Not found mrvl,intc-nr-irqs property\n");
		return;
	}

	/*
	 * Config all the interrupt source be able to interrupt the cpu 0,
	 * in IRQ mode, with priority 0 as masked by default.
	 */
	for (irq = 0; irq < nr_irqs; irq++)
		__raw_writel(ICU_IRQ_CPU0_MASKED, mmp_icu_base + (irq << 2));

	/* ICU is only used as wakeup logic,  disable the icu global mask. */
	i = 0;
	while (1) {
		u32 offset, val;
		if (of_property_read_u32_index(node, "mrvl,intc-gbl-mask",
						i++, &offset))
			break;
		if (of_property_read_u32_index(node, "mrvl,intc-gbl-mask",
						i++, &val)) {
			pr_warn("The params should keep pair!!!\n");
			break;
		}

		writel_relaxed(val, mmp_icu_base + offset);
	}

	/* Get the irq lines for cp */
	i = 0;
	while (!of_property_read_u32_index(node, "mrvl,intc-for-cp",
						i, &irq_for_cp[i])) {
		writel_relaxed(ICU_INT_CONF_SEAGULL,
				mmp_icu_base + (irq_for_cp[i] << 2));
		i++;
	}
	irq_for_cp_nr = i;

	/* Get the irq lines for sp */
	i = 0;
	while (!of_property_read_u32_index(node, "mrvl,intc-for-sp",
					i, &irq_for_sp[i])) {
		writel_relaxed(ICU_INT_CONF_SP | ICU_INT_CONF_PRIO(1),
				mmp_icu_base + (irq_for_sp[i] << 2));
		i++;
	}
	irq_for_sp_nr = i;

	/*
	 * Get wake clr devices info.
	 */
	wake_clr_devs = of_get_property(node, "mrvl,intc-wake-clr", &size);
	if (!wake_clr_devs)
		pr_warning("Not found mrvl,intc-wake-clr property\n");
	else {
		icu_data[0].nr_wake_clr = size / sizeof(struct icu_wake_clr);
		icu_data[0].wake_clr = kmalloc(size, GFP_KERNEL);
		if (!icu_data[0].wake_clr)
			return;

		i = 0;
		j = 0;
		while (j < icu_data[0].nr_wake_clr) {
			icu_data[0].wake_clr[j].irq = be32_to_cpup(wake_clr_devs + i++);
			icu_data[0].wake_clr[j++].mask = be32_to_cpup(wake_clr_devs + i++);
		}

		apmu_virt_addr = of_iomap(node, 1);
		if (!apmu_virt_addr) {
			pr_err("Failed to get apmu register base address!\n");
			return;
		}
	}

	/*
	 * Other initilization.
	 */
	icu_data[0].conf_enable = mmp_conf.conf_enable;
	icu_data[0].conf_disable = mmp_conf.conf_disable;
	icu_data[0].conf_mask = mmp_conf.conf_mask;
	icu_data[0].nr_irqs = nr_irqs;
	icu_data[0].virq_base = 32;

	gic_arch_extn.irq_mask = icu_mask_irq_wakeup;
	gic_arch_extn.irq_unmask = icu_unmask_irq_wakeup;
	gic_arch_extn.irq_set_affinity = icu_set_affinity;
	gic_arch_extn.irq_eoi = icu_eoi_clr_wakeup;

	return;
}

#endif
