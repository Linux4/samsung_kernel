/*
 *  linux/arch/arm/mach-mmp/irq-mmp2.c
 *
 *  Generic IRQ handling, GPIO IRQ demultiplexing, etc.
 *
 *  Author:	Haojian Zhuang <haojian.zhuang@marvell.com>
 *  Copyright:	Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/interrupt.h>

#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/hardware/gic.h>
#include <mach/regs-icu.h>
#include <mach/irqs.h>
#include <plat/mfp.h>

#include "common.h"

struct icu_mux_irq_chip_data {
	void __iomem	*mask;
	void __iomem	*status;
	unsigned int	base;
};

static void icu_mux_mask_irq(struct irq_data *d)
{
	struct icu_mux_irq_chip_data *chip_data = irq_data_get_irq_chip_data(d);
	u32 r;

	if (!chip_data) {
		printk(KERN_ERR "Can not find chip data for mux irq %d\n",
			d->irq);
		return;
	}
	r = __raw_readl(chip_data->mask) | (1 << (d->irq - chip_data->base));
	__raw_writel(r, chip_data->mask);
}

static void icu_mux_unmask_irq(struct irq_data *d)
{
	struct icu_mux_irq_chip_data *chip_data = irq_data_get_irq_chip_data(d);
	u32 r;

	if (!chip_data) {
		printk(KERN_ERR "Can not find chip data for mux irq %d\n",
			d->irq);
		return;
	}
	r = __raw_readl(chip_data->mask) & ~(1 << (d->irq - chip_data->base));
	__raw_writel(r, chip_data->mask);
}

#define DEFINE_ICU_MUX_IRQ(_name_, irq_base, prefix)			\
static struct icu_mux_irq_chip_data  _name_##_icu_chip_data = {	\
	.mask		= (void __iomem *)prefix##_MASK,		\
	.status		= (void __iomem *)prefix##_STATUS,		\
	.base		= irq_base,					\
};									\
									\
static void _name_##_irq_demux(unsigned int irq, struct irq_desc *desc)	\
{									\
	unsigned long status, mask, n;					\
	struct irq_chip *chip = irq_get_chip(irq);			\
									\
	chained_irq_enter(chip, desc);					\
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
	chained_irq_exit(chip, desc);					\
}									\

int mmp3_set_wake(struct irq_data *data, unsigned int on)
{
	unsigned int irq = data->irq;
	struct irq_desc *desc = irq_to_desc(irq);

	if (unlikely(irq >= nr_irqs)) {
		pr_err("IRQ nubmers are out of boundary!\n");
		return -EINVAL;
	}

	if (on) {
		if (desc->action)
			desc->action->flags |= IRQF_NO_SUSPEND;
	} else {
		if (desc->action)
			desc->action->flags &= ~IRQF_NO_SUSPEND;
	}

	/*
	 * TODO: Setup wakeup source. Will do it when wakeup is
	 * ready in hardware side.
	 */

	return 0;
}

static struct irq_chip icu_mux_irq_chip = {
	.name		= "icu mux",
	.irq_mask	= icu_mux_mask_irq,
	.irq_unmask	= icu_mux_unmask_irq,
	.irq_disable	= icu_mux_mask_irq,
#if CONFIG_PM
	.irq_set_wake	= mmp3_set_wake,
#endif
};

DEFINE_ICU_MUX_IRQ(pmic,	IRQ_MMP3_PMIC_BASE,	MMP3_ICU_INT_4);
DEFINE_ICU_MUX_IRQ(rtc,		IRQ_MMP3_RTC_BASE,	MMP3_ICU_INT_5);
DEFINE_ICU_MUX_IRQ(hsi3,	IRQ_MMP3_HSI3_BASE,	MMP3_ICU_INT_6);
DEFINE_ICU_MUX_IRQ(gpu,		IRQ_MMP3_GPU_BASE,	MMP3_ICU_INT_8);
DEFINE_ICU_MUX_IRQ(twsi,	IRQ_MMP3_TWSI_BASE,	MMP3_ICU_INT_17);
DEFINE_ICU_MUX_IRQ(hsi2,	IRQ_MMP3_HSI2_BASE,	MMP3_ICU_INT_18);
DEFINE_ICU_MUX_IRQ(dxo,		IRQ_MMP3_DXO_BASE,	MMP3_ICU_INT_30);
/*DEFINE_ICU_MUX_IRQ(misc1,	IRQ_MMP3_MISC1_BASE,	MMP3_ICU_INT_35);*/
DEFINE_ICU_MUX_IRQ(ci,		IRQ_MMP3_CI_BASE,	MMP3_ICU_INT_42);
DEFINE_ICU_MUX_IRQ(ssp,		IRQ_MMP3_SSP_BASE,	MMP3_ICU_INT_51);
DEFINE_ICU_MUX_IRQ(hsi1,	IRQ_MMP3_HSI1_BASE,	MMP3_ICU_INT_55);
DEFINE_ICU_MUX_IRQ(misc2,	IRQ_MMP3_MISC2_BASE,	MMP3_ICU_INT_57);
DEFINE_ICU_MUX_IRQ(hsi0,	IRQ_MMP3_HSI0_BASE,	MMP3_ICU_INT_58);
DEFINE_ICU_MUX_IRQ(dma,		IRQ_MMP3_DMA_BASE,	MMP3_ICU_DMA_IRQ0);

static void init_mux_irq(struct icu_mux_irq_chip_data *chip_data,
	int mux_irq, int mux_start, int count, irq_flow_handler_t mux_handle)
{
	struct irq_chip *chip;
	int irq;
	u32 r;
	unsigned int irq_base;

	irq_base = irq_alloc_descs(mux_start, mux_start, count, numa_node_id());
	if (IS_ERR_VALUE(irq_base)) {
		WARN(1, "Cannot allocate irq_descs @ IRQ%d, assuming pre-allocated\n",
		     mux_start);
		return;
	}

	/* maks all the irqs*/
	r = __raw_readl(chip_data->mask) | ((1 << count) - 1);
	__raw_writel(r, chip_data->mask);

	for (irq = mux_start; count > 0; irq++, count--) {
		irq_set_chip_and_handler(irq, &icu_mux_irq_chip,
			handle_level_irq);
		irq_set_chip_data(irq, chip_data);
		set_irq_flags(irq, IRQF_VALID | IRQF_PROBE);
	}

	chip = irq_get_chip(mux_irq);
	irq_set_chained_handler(mux_irq, mux_handle);
	chip->irq_unmask(irq_get_irq_data(mux_irq));
}

static void pmic_irq_ack(struct irq_data *d)
{
	unsigned long data;
	if (d->irq == IRQ_MMP3_PMIC) {
		data = mfp_read(MFP_PIN_PMIC_INT);
		mfp_write(MFP_PIN_PMIC_INT, data | (0x1 << 6));
		mfp_write(MFP_PIN_PMIC_INT, data & ~(0x1 << 6));
	}
}

static void pmic_set_ack(int mux_irq)
{
	struct irq_chip *chip;

	chip = irq_get_chip(mux_irq);
	chip->irq_ack = pmic_irq_ack;
}

static void icu1_mask_irq(int irq)
{
	uint32_t r = __raw_readl(ICU1_INT_CONF(irq));

	r &= ~ICU1_INT_ROUTE_PJMP1_IRQ;
	r &= ~ICU1_INT_ROUTE_PJMP1_FIQ;
	__raw_writel(r, ICU1_INT_CONF(irq));
}

static void icu2_mask_irq(int irq)
{
	uint32_t r = __raw_readl(ICU2_INT_CONF(irq));

	r &= ~ICU2_INT_ROUTE_PJMP2_IRQ;
	r &= ~ICU2_INT_ROUTE_PJMP2_FIQ;
	r &= ~ICU2_INT_ROUTE_PJMM_IRQ;
	r &= ~ICU2_INT_ROUTE_PJMM_FIQ;
	__raw_writel(r, ICU2_INT_CONF(irq));
}

void __init mmp3_init_gic(void)
{
	int i;

	/* disable global irq of ICU for MP1, MP2, MM*/
	__raw_writel(0x1, MMP3_ICU_GBL_IRQ1_MSK);
	__raw_writel(0x1, MMP3_ICU_GBL_IRQ2_MSK);
	__raw_writel(0x1, MMP3_ICU_GBL_IRQ3_MSK);
	__raw_writel(0x1, MMP3_ICU_GBL_IRQ4_MSK);
	__raw_writel(0x1, MMP3_ICU_GBL_IRQ5_MSK);
	__raw_writel(0x1, MMP3_ICU_GBL_IRQ6_MSK);

	/*
	 * FIXME
	 * It is required to unmask ICU1_INT_35_MASK_NPMUIRQ_[0-2], 24-22
	 * bit to enable IRQ_MMP3_PMU_CPU[0-2] (via sysint[86-88])
	 */
	{
		int val;
		val = __raw_readl(MMP3_ICU_INT_35_MASK);
		val &= ~((1<<24) | (1<<23) | (1<<22));
		__raw_writel(val, MMP3_ICU_INT_35_MASK);
	}


	for (i = 0; i < 64; i++) {
		icu1_mask_irq(i);
		icu2_mask_irq(i);
	}

	gic_arch_extn.irq_set_wake = mmp3_set_wake;

	init_mux_irq(&pmic_icu_chip_data, IRQ_MMP3_PMIC_MUX,
			IRQ_MMP3_PMIC_BASE, 4, pmic_irq_demux);
	init_mux_irq(&rtc_icu_chip_data, IRQ_MMP3_RTC_MUX,
			IRQ_MMP3_RTC_BASE, 2, rtc_irq_demux);
	init_mux_irq(&hsi3_icu_chip_data, IRQ_MMP3_HSI3_MUX,
			IRQ_MMP3_HSI3_BASE, 3, hsi3_irq_demux);
	init_mux_irq(&gpu_icu_chip_data, IRQ_MMP3_GPU_MUX,
			IRQ_MMP3_GPU_BASE, 3, gpu_irq_demux);
	init_mux_irq(&twsi_icu_chip_data, IRQ_MMP3_TWSI_MUX,
			IRQ_MMP3_TWSI_BASE, 5, twsi_irq_demux);
	init_mux_irq(&hsi2_icu_chip_data, IRQ_MMP3_HSI2_MUX,
			IRQ_MMP3_HSI2_BASE, 2, hsi2_irq_demux);
	init_mux_irq(&dxo_icu_chip_data, IRQ_MMP3_DXO_MUX,
			IRQ_MMP3_DXO_BASE, 2, dxo_irq_demux);
	/*init_mux_irq(&misc1_icu_chip_data, IRQ_MMP3_MISC1_MUX,
			IRQ_MMP3_MISC1_BASE, 31, misc1_irq_demux);*/
	init_mux_irq(&ci_icu_chip_data, IRQ_MMP3_CI_MUX,
			IRQ_MMP3_CI_BASE, 2, ci_irq_demux);
	init_mux_irq(&ssp_icu_chip_data, IRQ_MMP3_SSP_MUX,
			IRQ_MMP3_SSP_BASE, 2, ssp_irq_demux);
	init_mux_irq(&hsi1_icu_chip_data, IRQ_MMP3_HSI1_MUX,
			IRQ_MMP3_HSI1_BASE, 4, hsi1_irq_demux);
	init_mux_irq(&misc2_icu_chip_data, IRQ_MMP3_MISC2_MUX,
			IRQ_MMP3_MISC2_BASE, 20, misc2_irq_demux);
	init_mux_irq(&hsi0_icu_chip_data, IRQ_MMP3_HSI0_MUX,
			IRQ_MMP3_HSI0_BASE, 5, hsi0_irq_demux);
	init_mux_irq(&dma_icu_chip_data, IRQ_MMP3_DMA_RIQ,
			IRQ_MMP3_DMA_BASE, 24, dma_irq_demux);

	/*
	 * Note: IRQ_MMP3_PMIC requires the PMIC MFPR register
	 * to be written to clear the interrupt.
	 */
	pmic_set_ack(IRQ_MMP3_PMIC);

}
