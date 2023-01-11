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
#include <linux/ioport.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/slab.h>
#include "irqs-pxa1928.h"

#define ICU_IRQ_START	IRQ_PXA1928_START
#define ICU_IRQ_END	(IRQ_PXA1928_START + 64)

static void __iomem *apmu_virt_addr;
static void __iomem *mmp_icu_base;

static u32 irq_for_sp[64];
static u32 irq_for_sp_nr;	/* How many irqs will be routed to cp */

struct pxa1928_icu_mux {
	u32 icu_first_irq;
	struct list_head s_irq_list;
	unsigned int mask_reg;
	unsigned int mask;
};

struct pxa1928_secondary_irq_info {
	u32 nr;
	u32 parent;
	u32 mask_offset;
	u32 affinity;
	struct list_head node;
};

#define WAKE_CLR_OFFSET		0x7c
struct icu_wake_clr {
	int	irq;
	int	mask;
};

struct	icu_wake_clr	*wake_clr;
int	nr_wake_clr;

static struct pxa1928_icu_mux icu_mux[] = {
	{
		.icu_first_irq = IRQ_PXA1928_INT4,
		.mask_reg = PXA1928_ICU_INT_4_MASK,
		.mask = 0x0000000b,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT5,
		.mask_reg = PXA1928_ICU_INT_5_MASK,
		.mask = 0x00000003,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT8,
		.mask_reg = PXA1928_ICU_INT_8_MASK,
		.mask = 0x0000000d,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT17,
		.mask_reg = PXA1928_ICU_INT_17_MASK,
		.mask = 0x0000003f,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT18,
		.mask_reg = PXA1928_ICU_INT_18_MASK,
		.mask = 0x00000004,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT30,
		.mask_reg = PXA1928_ICU_INT_30_MASK,
		.mask = 0x00000007,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT35,
		.mask_reg = PXA1928_ICU_INT_35_MASK,
		.mask = 0x803fffff,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT42,
		.mask_reg = PXA1928_ICU_INT_42_MASK,
		.mask = 0x00000003,
	}, {
		.icu_first_irq = IRQ_PXA1928_DMA_IRQ,
		.mask_reg = PXA1928_ICU_DMA_IRQ1_MASK,
		.mask = 0x007fffff,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT51,
		.mask_reg = PXA1928_ICU_INT_51_MASK,
		.mask = 0x00000003,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT55,
		.mask_reg = PXA1928_ICU_INT_55_MASK,
		.mask = 0x00000001,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT57,
		.mask_reg = PXA1928_ICU_INT_57_MASK,
		.mask = 0x00806efc,
	}, {
		.icu_first_irq = IRQ_PXA1928_INT58,
		.mask_reg = PXA1928_ICU_INT_58_MASK,
		.mask = 0x00000001,
	},
};

#define PXA1928_ICU_MUX_NR		ARRAY_SIZE(icu_mux)

static struct pxa1928_secondary_irq_info s_irq_info[] = {
	{
		.nr = IRQ_PXA1928_CHARGER,
		.parent = IRQ_PXA1928_INT4,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_PMIC,
		.parent = IRQ_PXA1928_INT4,
		.mask_offset = 1,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_CHRG_DTC_OUT,
		.parent = IRQ_PXA1928_INT4,
		.mask_offset = 3,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_RTC_ALARM,
		.parent = IRQ_PXA1928_INT5,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_RTC,
		.parent = IRQ_PXA1928_INT5,
		.mask_offset = 1,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_GC2300,
		.parent = IRQ_PXA1928_INT8,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_GC320,
		.parent = IRQ_PXA1928_INT8,
		.mask_offset = 2,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_INT_CPU3,
		.parent = IRQ_PXA1928_INT8,
		.mask_offset = 3,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_TWSI2,
		.parent = IRQ_PXA1928_INT17,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_TWSI3,
		.parent = IRQ_PXA1928_INT17,
		.mask_offset = 1,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_TWSI4,
		.parent = IRQ_PXA1928_INT17,
		.mask_offset = 2,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_TWSI5,
		.parent = IRQ_PXA1928_INT17,
		.mask_offset = 3,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_TWSI6,
		.parent = IRQ_PXA1928_INT17,
		.mask_offset = 4,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_INT_CPU2,
		.parent = IRQ_PXA1928_INT17,
		.mask_offset = 5,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_INT_CPU1,
		.parent = IRQ_PXA1928_INT18,
		.mask_offset = 2,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_ISP_DMA,
		.parent = IRQ_PXA1928_INT30,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DXO_ISP,
		.parent = IRQ_PXA1928_INT30,
		.mask_offset = 1,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_INT_CPU0,
		.parent = IRQ_PXA1928_INT30,
		.mask_offset = 2,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MP_COMMTX0,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 4,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MP_COMMTX1,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 5,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MP_COMMTX2,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 6,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MP_COMMTX3,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 7,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MP_COMMRX0,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 8,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MP_COMMRX1,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 9,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MP_COMMRX2,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 10,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MP_COMMRX3,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 11,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_CTI_NCTIIRQ0,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 12,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_CTI_NCTIIRQ1,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 13,
		.affinity = 0,
	},  {
		.nr = IRQ_PXA1928_MULTICORE_CTI_NCTIIRQ2,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 14,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_CTI_NCTIIRQ3,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 15,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_NPMUIRQ0,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 16,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_NPMUIRQ1,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 17,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_NPMUIRQ2,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 18,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_NPMUIRQ3,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 19,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_CORE_MP_NAXIERRIRQ,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 20,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_WDT2_INT,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 21,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MCK_PML_OVERFLOW,
		.parent = IRQ_PXA1928_INT35,
		.mask_offset = 31,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_CCIC2,
		.parent = IRQ_PXA1928_INT42,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_CCIC1,
		.parent = IRQ_PXA1928_INT42,
		.mask_offset = 1,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT0,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT1,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 1,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT2,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 2,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT3,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 3,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT4,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 4,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT5,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 5,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT6,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 6,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT7,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 7,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT8,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 8,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT9,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 9,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT10,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 10,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT11,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 11,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT12,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 12,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT13,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 13,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT14,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 14,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_DMA_CHNL_INT15,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 15,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MDMA_CHNL_INT0,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 16,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MDMA_CHNL_INT1,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 17,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_ADMA_CHNL_INT0,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 18,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_ADMA_CHNL_INT1,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 19,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_ADMA_CHNL_INT2,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 20,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_ADMA_CHNL_INT3,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 21,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_VDMA_INT,
		.parent = IRQ_PXA1928_DMA_IRQ,
		.mask_offset = 22,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_SSP1_SRDY,
		.parent = IRQ_PXA1928_INT51,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_SSP3_SRDY,
		.parent = IRQ_PXA1928_INT51,
		.mask_offset = 1,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_INT55,
		.parent = IRQ_PXA1928_INT55,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_RIPC_INT0,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 0,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_RIPC_INT1,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 1,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_RIPC_INT2,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 2,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_RIPC_INT3,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 3,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_NCOMMIRQ0,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 4,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_NCOMMIRQ1,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 5,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_NCOMMIRQ2,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 6,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MULTICORE_NCOMMIRQ3,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 7,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_GPIO_INT,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 12,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_THERMAL_SENSOR,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 13,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_MAIN_PMU_INT,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 14,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_APMU_INT,
		.parent = IRQ_PXA1928_INT57,
		.mask_offset = 23,
		.affinity = 0,
	}, {
		.nr = IRQ_PXA1928_INT58,
		.parent = IRQ_PXA1928_INT58,
		.mask_offset = 0,
		.affinity = 0,
	},
};

#define PXA1928_SECONDARY_IRQ_NR	ARRAY_SIZE(s_irq_info)

static cpumask_var_t wakeupgen_irq_affinity[64];

void __iomem *pxa1928_icu_get_base_addr(void)
{
	return mmp_icu_base;
}

/*
 * Return:
 * >=0: the irq line for ICU
 * -1: no need to handle this irq line
 */
static int irq_need_handle(struct irq_data *d)
{
	struct pxa1928_secondary_irq_info *info;
	int irq = d->irq;
	int i;

	/* Avoid modify SP interrupts */
	for (i = 0; i < irq_for_sp_nr; i++)
		if (irq_for_sp[i] == (irq - IRQ_PXA1928_START))
			return 1;

	/* out of the ICU range */
	if (irq >= IRQ_PXA1928_END)
		return -1;

	switch (irq) {
	case IRQ_PXA1928_KEYPAD:
		irq = IRQ_PXA1928_KEYPAD_ICU;
		goto back;
	case IRQ_PXA1928_USB_OTG:
		irq = IRQ_PXA1928_USB_OTG_ICU;
		goto back;
	case IRQ_PXA1928_MMC1:
		irq = IRQ_PXA1928_MMC1_ICU;
		goto back;
	case IRQ_PXA1928_MMC2:
		irq = IRQ_PXA1928_MMC2_ICU;
		goto back;
	case IRQ_PXA1928_MMC3:
		irq = IRQ_PXA1928_MMC3_ICU;
		goto back;
	case IRQ_PXA1928_MMC4:
		irq = IRQ_PXA1928_MMC4_ICU;
		goto back;
	default:
		break;
	}

	/* irq 55 & 58 are < ICU_IRQ_END
	 * but they are muxed
	 */
	if ((irq >= ICU_IRQ_END) ||
		(irq == IRQ_PXA1928_INT55) ||
		(irq == IRQ_PXA1928_INT58)) {
		for (i = 0; i < PXA1928_ICU_MUX_NR; i++) {
			list_for_each_entry(
				info, &icu_mux[i].s_irq_list, node) {
				if (irq == info->nr) {
					irq = icu_mux[i].icu_first_irq;
					goto back;
				}
			}
		}

		if (i == PXA1928_ICU_MUX_NR)
			return -1;
	}

	/*
	 * the icu connects the sys_int_ap interrupt source to the SPI interface
	 * of the GIC. since the GIC SPI interrupts start from 32, here minus 32
	 * from the GIC spi irq number to get the raw icu based irq.
	 * For PPI and SGI, just skip it.
	 */
back:
	return irq - IRQ_PXA1928_START;
}

static void icu_mask_irq(struct irq_data *d)
{
	struct pxa1928_secondary_irq_info *info;
	void __iomem *reg;
	unsigned long val;
	int irq, i;
	int offset = -1;
	void __iomem *icu_base;

	irq = irq_need_handle(d);
	if ((irq < 0) || (irq >= 64))
		return;

	icu_base = pxa1928_icu_get_base_addr();
	for (i = 0; i < PXA1928_ICU_MUX_NR; i++) {
		/* when we are trying to mask a secondary irq */
		if ((irq == (icu_mux[i].icu_first_irq - IRQ_PXA1928_START)) &&
				(d->irq >= ICU_IRQ_END)) {
			/* find out the bit offset of the secondary mask reg */
			list_for_each_entry(
				info, &icu_mux[i].s_irq_list, node) {
				if (d->irq == info->nr) {
					offset = info->mask_offset;
					break;
				}
			}

			if (offset == -1)
				return;

			reg = icu_base + icu_mux[i].mask_reg;
			val = __raw_readl(reg);
			val |= (1 << offset);
			__raw_writel(val, reg);
			/* if all the secondary irqs are masked,
			 * break the loop and mask the first level irq.
			 * otherwise, return. */
			if ((val & icu_mux[i].mask) == icu_mux[i].mask)
				break;
			else
				return;
		}
	}

	/* if we are trying to mask a first level irq */
	reg = ICU_INT_CONF(irq);
	val = __raw_readl(reg);
	val &= ~PXA1928_ICU_INT_CONF_AP_MASK;
	__raw_writel(val, reg);
}

static void icu_unmask_irq(struct irq_data *d)
{
	struct pxa1928_secondary_irq_info *info;
	void __iomem *reg;
	unsigned long val;
	unsigned int cpu;
	int irq, i;
	int offset = -1;
	void __iomem *icu_base;

	irq = irq_need_handle(d);
	if ((irq < 0) || (irq >= 64))
		return;

	cpu = cpumask_first(d->affinity);
	icu_base = pxa1928_icu_get_base_addr();

	for (i = 0; i < PXA1928_ICU_MUX_NR; i++) {
		/* when we are trying to unmask a secondary irq */
		if ((irq == (icu_mux[i].icu_first_irq - IRQ_PXA1928_START)) &&
				(d->irq != irq)) {
			/* find out the bit offset of the secondary mask reg */
			list_for_each_entry(
				info, &icu_mux[i].s_irq_list, node) {
				if (d->irq == info->nr) {
					offset = info->mask_offset;
					break;
				}
			}

			if (offset == -1)
				return;

			reg = icu_base + icu_mux[i].mask_reg;
			val = __raw_readl(reg);
			val &= ~(1 << offset);
			__raw_writel(val, reg);
			/* break the loop and unmask the first level irq. */
			break;
		}
	}

	reg = ICU_INT_CONF(irq);
	val = __raw_readl(reg);
	val &= ~PXA1928_ICU_INT_CONF_AP_MASK;
	val |= PXA1928_ICU_INT_CONF_AP(cpu);
	__raw_writel(val, reg);
}

/* detect the affinity conflict between
 * secondary irqs and first level irqs */
static int icu_set_affinity(struct irq_data *d,
	const struct cpumask *mask_val, bool force)
{
	int irq, i;

	irq = irq_need_handle(d);
	if ((irq < 0) || (irq >= 64))
		return -EINVAL;

	for (i = 0; i < PXA1928_ICU_MUX_NR; i++) {
		/* when we are trying to set affinity of a secondary irq */
		if ((irq == (icu_mux[i].icu_first_irq - IRQ_PXA1928_START)) &&
				(d->irq != irq)) {
			if (cpumask_first(wakeupgen_irq_affinity[irq]) !=
					cpumask_first(mask_val)) {
				WARN(1, "icu_set_affinity: Secondary IRQ affinity conflict with First Level IRQ\n");
			}
			break;
		}
	}

	return 0;
}

static void icu_eoi_clr_wakeup(struct irq_data *d)
{
	int irq;
	u32 val;
	int i;

	irq = irq_need_handle(d);
	if ((irq < 0) || (irq >= 64))
		return;

	for (i = 0; i < nr_wake_clr; i++) {
		if (irq == wake_clr[i].irq) {
			val = readl_relaxed(apmu_virt_addr + WAKE_CLR_OFFSET);
			writel_relaxed(val | wake_clr[i].mask,
				apmu_virt_addr + WAKE_CLR_OFFSET);
			break;
		}
	}

	return;
}

#define PMUA_COREAP_DEBUG	0x3E0
#define MSK_CORE_GIC_INT	(1 << 2)
static int pmua_mask_core_gic_int(void)
{
	int i;

	for (i = 0; i < CONFIG_NR_CPUS; i++) {
		u32 regdbg = readl(apmu_virt_addr + PMUA_COREAP_DEBUG + i * 4);
		regdbg |= MSK_CORE_GIC_INT;
		writel(regdbg, apmu_virt_addr + PMUA_COREAP_DEBUG + i * 4);
	}

	return 0;
}

void __init pxa1928_of_wakeup_init(void)
{
	struct device_node *node;
	struct irq_data	d;
	const __be32 *wakeup_reg;
	const __be32 *sp_irq_reg;
	const __be32 *wake_clr_devs;
	int size, i = 0, j = 0;
	int irq;

	node = of_find_compatible_node(NULL, NULL, "mrvl,pxa1928-intc-wakeupgen");
	if (!node)
		return;
	pr_info("Get interrupt controller in arch-pxa1928\n");

	mmp_icu_base = of_iomap(node, 0);
	if (!mmp_icu_base) {
		pr_err("Failed to get interrupt controller register\n");
		return;
	}

	/* ICU is only used as wake up logic
	  * disable the global irq/fiq in icu for all cores.
	  */
	wakeup_reg = of_get_property(node, "mrvl,intc-gbl-mask", &size);
	if (!wakeup_reg) {
		pr_err("Not found mrvl,intc-gbl-mask property\n");
		return;
	}

	size /= sizeof(*wakeup_reg);
	while (i < size) {
		unsigned offset, val;

		offset = be32_to_cpup(wakeup_reg + i++);
		val = be32_to_cpup(wakeup_reg + i++);
		writel_relaxed(val, mmp_icu_base + offset);
	}

	/* Get the irq lines and ignore irqs */
	sp_irq_reg = of_get_property(node, "mrvl,intc-for-sp", &size);
	if (!sp_irq_reg)
		return;

	irq_for_sp_nr = size / sizeof(*sp_irq_reg);
	for (i = 0; i < irq_for_sp_nr; i++)
		irq_for_sp[i] = be32_to_cpup(sp_irq_reg + i);

	/*
	 * Get wake clr devices info.
	 */
	wake_clr_devs = of_get_property(node, "mrvl,intc-wake-clr", &size);
	if (!wake_clr_devs)
		pr_warn("Not found mrvl,intc-wake-clr property\n");
	else {
		nr_wake_clr = size / sizeof(struct icu_wake_clr);
		wake_clr = kmalloc(size, GFP_KERNEL);
		if (!wake_clr)
			return;

		i = 0;
		j = 0;
		while (j < nr_wake_clr) {
			wake_clr[j].irq = be32_to_cpup(wake_clr_devs + i++);
			wake_clr[j++].mask = be32_to_cpup(wake_clr_devs + i++);
		}

		apmu_virt_addr = of_iomap(node, 1);
		if (!apmu_virt_addr) {
			pr_err("Failed to get apmu register base address!\n");
			return;
		}
	}

	for (i = 0; i < PXA1928_ICU_MUX_NR; i++) {
		INIT_LIST_HEAD(&icu_mux[i].s_irq_list);
		for (j = 0; j < PXA1928_SECONDARY_IRQ_NR; j++) {
			if (icu_mux[i].icu_first_irq == s_irq_info[j].parent)
				list_add_tail(&s_irq_info[j].node,
					&icu_mux[i].s_irq_list);
		}
	}

	/* init local irq affinity to core 0 */
	for (irq = 0; irq < 64; irq++)
		cpumask_set_cpu(0, wakeupgen_irq_affinity[irq]);
	/* mask all irqs except for the irqs for SP */
	for (irq = IRQ_PXA1928_START; irq < IRQ_PXA1928_END; irq++) {
		if (irq == IRQ_PXA1928_TIMER1 || irq == IRQ_PXA1928_TIMER2 ||
			irq == IRQ_PXA1928_TIMER3)
			continue;
		d.irq = irq;
		icu_mask_irq(&d);
	}

	gic_arch_extn.irq_mask = icu_mask_irq;
	gic_arch_extn.irq_unmask = icu_unmask_irq;
	gic_arch_extn.irq_set_affinity = icu_set_affinity;
	gic_arch_extn.irq_eoi = icu_eoi_clr_wakeup;

	/* mask gic signals to APMU */
	pmua_mask_core_gic_int();

	return;
}

int __init arch_early_irq_init(void)
{
	pxa1928_of_wakeup_init();
	return 0;
}
