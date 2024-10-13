/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * Contact: steve.zhan <steve.zhan@spreadtrum.com>
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>

#include <linux/irqchip/arm-gic.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/sci_glb_regs.h>
#include <mach/sci.h>
#include <mach/arch_misc.h>
#include <asm/fiq.h>
#include <mach/adi.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/irqchip.h>
#endif
/* general interrupt registers */
#define	INTC_IRQ_MSKSTS		(0x0000)
#define	INTC_IRQ_RAW		(0x0004)
#define	INTC_IRQ_EN		(0x0008)
#define	INTC_IRQ_DIS		(0x000C)
#define	INTC_IRQ_SOFT		(0x0010)
#define	INTC_FIQ_STS		(0x0020)
#define	INTC_FIQ_EN		(0x0028)
#define	INTC_FIQ_DIS		(0x002C)

struct intc {
	u32 intc_reg_base;
	u32 min_int_num;/*global logical number max value in intc*/
	u32 max_min_num;/*global logical number min value in intc */
	u32 min_bit_offset;/*It is equal to "min_int_num - actual bit offset"*/
};

struct intc_mux_irq {/*multi irqs mapping to one bit*/
	u32 irq_number;
	u32 intc_base;
	u32 bits;
	u32 is_unmask;
};

#if defined(CONFIG_ARCH_SCX35)
/*_intc and mux_irq is separately, we just simply enable/disable it at the same time
   When system deepsleep, only mux_irq(AON) can wakeup system(intc0,1,2,3,gic,armcore)
   intc0/1/2/3 will lose if system in deepsleep, _mux can wakeup. intc0/1/2/3 need SW retention.
   gic clock will stop if all of core has been wfi.intc0/1/2/3 can wakeup.
*/
static const struct intc _intc[] = {
		{SPRD_INTC0_BASE, 2, 31, 0},
		{SPRD_INTC1_BASE, 34, 63, 32},
		{SPRD_INTC2_BASE, 66, 95, 64},
		{SPRD_INTC3_BASE, 98, 124, 96},
};
static struct intc_mux_irq _mux[] = {
		{30,SPRD_INT_BASE,2,0},{31,SPRD_INT_BASE,2,0},{28,SPRD_INT_BASE,2,0},
		{121,SPRD_INT_BASE,2,0},{120,SPRD_INT_BASE,2,0},{119,SPRD_INT_BASE,2,0},
		{118,SPRD_INT_BASE,2,0},{29,SPRD_INT_BASE,2,0},
		{21,SPRD_INT_BASE,3,0},{22,SPRD_INT_BASE,3,0},{23,SPRD_INT_BASE,3,0},{24,SPRD_INT_BASE,3,0},
		{35,SPRD_INT_BASE,4,0},{36,SPRD_INT_BASE,4,0},{38,SPRD_INT_BASE,4,0},{25,SPRD_INT_BASE,4,0},
		{27,SPRD_INT_BASE,4,0},{20,SPRD_INT_BASE,4,0},{34,SPRD_INT_BASE,4,0},
		{41,SPRD_INT_BASE,5,0},{40,SPRD_INT_BASE,5,0},{42,SPRD_INT_BASE,5,0},{44,SPRD_INT_BASE,5,0},
		{45,SPRD_INT_BASE,5,0},{43,SPRD_INT_BASE,5,0},
		{39,SPRD_INT_BASE,6,0},
		{85,SPRD_INT_BASE,7,0},{84,SPRD_INT_BASE,7,0},{83,SPRD_INT_BASE,7,0},
		{67,SPRD_INT_BASE,8,0},{68,SPRD_INT_BASE,8,0},{69,SPRD_INT_BASE,8,0},
		{70,SPRD_INT_BASE,9,0},{71,SPRD_INT_BASE,9,0},{72,SPRD_INT_BASE,9,0},
		{73,SPRD_INT_BASE,10,0},{74,SPRD_INT_BASE,10,0},
		{123,SPRD_INT_BASE,11,0},{124,SPRD_INT_BASE,11,0},
		{26,SPRD_INT_BASE,12,0},{122,SPRD_INT_BASE,12,0},
		{86,SPRD_INT_BASE,13,0},
		{37,SPRD_INT_BASE,14,0},
};

#define LEGACY_FIQ_BIT	(32)
#define LEGACY_IRQ_BIT	(29)

static __init void __irq_init(void)
{
	if (soc_is_scx35_v0())
		sci_glb_clr(REG_AP_AHB_AP_SYS_AUTO_SLEEP_CFG,BIT_CA7_CORE_AUTO_GATE_EN);

	/*enable all intc*/
	sci_glb_set(REG_AP_APB_APB_EB, BIT_INTC0_EB | BIT_INTC1_EB |
			BIT_INTC2_EB | BIT_INTC3_EB);
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_INTC_EB);

	/*disable default for startup*/
	__raw_writel(~0, SPRD_INTC0_BASE + INTC_IRQ_DIS);
	__raw_writel(~0, SPRD_INTC0_BASE + INTC_FIQ_DIS);
	__raw_writel(~0, SPRD_INTC1_BASE + INTC_IRQ_DIS);
	__raw_writel(~0, SPRD_INTC1_BASE + INTC_FIQ_DIS);
	__raw_writel(~0, SPRD_INTC2_BASE + INTC_IRQ_DIS);
	__raw_writel(~0, SPRD_INTC2_BASE + INTC_FIQ_DIS);
	__raw_writel(~0, SPRD_INTC3_BASE + INTC_IRQ_DIS);
	__raw_writel(~0, SPRD_INTC3_BASE + INTC_FIQ_DIS);
	__raw_writel(~0, SPRD_INT_BASE + INTC_IRQ_DIS);
	__raw_writel(~0, SPRD_INT_BASE + INTC_FIQ_DIS);
}

#else

static const struct intc _intc[] = {
		{SPRD_INTC0_BASE, 0, 31, 0},
		{SPRD_INTC1_BASE, 32, 61, 32},
};

static struct intc_mux_irq _mux[] = {};

#define LEGACY_FIQ_BIT	(31)
#define LEGACY_IRQ_BIT	(28)

static __init void __irq_init(void)
{
	/*
	 * gic clock will be stopped after 2 cores enter standby in the same time,
	 * dsp assert if IRQ_DSP0_INT and IRQ_DSP1_INT are disabled. so enable IRQ_DSP0_INT
	 * and IRQ_DSP1_INT in INTC0 here.
	 */
	u32 val = __raw_readl(SPRD_INTC0_BASE + INTC_IRQ_EN);
	val |= (SCI_INTC_IRQ_BIT(IRQ_DSP0_INT) | SCI_INTC_IRQ_BIT(IRQ_DSP1_INT) | SCI_INTC_IRQ_BIT(IRQ_EPT_INT));
	val |= (SCI_INTC_IRQ_BIT(IRQ_SIM0_INT) | SCI_INTC_IRQ_BIT(IRQ_SIM1_INT));
	val |= (SCI_INTC_IRQ_BIT(IRQ_TIMER0_INT));
	__raw_writel(val, SPRD_INTC0_BASE + INTC_IRQ_EN);
}
#endif

static inline int __irq_mux_irq_find(u32 irq, u32 *base, u32 *bit, int *p_index)
{
	int s = ARRAY_SIZE(_mux);
	while (s--)
		if (_mux[s].irq_number == irq) {
			*base = _mux[s].intc_base;
			*bit = _mux[s].bits;
			*p_index = s;
			return 0;
		}
	return -ENXIO;
}

static inline int __irq_find_base(u32 irq, u32 *base, u32 *bit)
{
	int s = ARRAY_SIZE(_intc);
	while (s--)
		if (irq >= _intc[s].min_int_num && irq <= _intc[s].max_min_num) {
			*base = _intc[s].intc_reg_base;
			*bit = irq - _intc[s].min_bit_offset;
			return 0;
		}
	return -ENXIO;
}

static inline void __mux_irq(u32 irq, u32 offset, u32 is_unmask)
{
	u32 base, bit, s;
	int index = 0;
	int dont_mask = 0;
	if (!__irq_mux_irq_find(irq, &base, &bit, &index)) {
		_mux[index].is_unmask = !!is_unmask;

		if (is_unmask) {/*if any one need unmask(wakeup source), let the irq mux bit enable*/
			__raw_writel(1 << bit, base + offset);
		} else {/*maybe mask*/
			s = ARRAY_SIZE(_mux);
			while (s--) {
				if (_mux[s].is_unmask) {
					dont_mask = 1;
					break;
				}
			}
			if (!dont_mask)
				__raw_writel(1 << bit, base + offset);
		}
	}
}

void sci_intc_mask(u32 __irq)
{
	unsigned int irq = SCI_GET_INTC_IRQ(__irq);
	u32 base;
	u32 bit;
	u32 offset = INTC_IRQ_DIS;
#ifdef CONFIG_SPRD_WATCHDOG_SYS_FIQ
	if (unlikely(irq == (IRQ_CA7WDG_INT - IRQ_GIC_START)))
		offset = INTC_FIQ_DIS;
#endif
	__mux_irq(irq, offset, 0);
	if (!__irq_find_base(irq, &base, &bit))
		__raw_writel(1 << bit, base + offset);
}
EXPORT_SYMBOL(sci_intc_mask);

void sci_intc_unmask(u32 __irq)
{
	unsigned int irq = SCI_GET_INTC_IRQ(__irq);
	u32 base;
	u32 bit;
	u32 offset = INTC_IRQ_EN;
#ifdef CONFIG_SPRD_WATCHDOG_SYS_FIQ
	if (unlikely(irq == (IRQ_CA7WDG_INT - IRQ_GIC_START)))
		offset = INTC_FIQ_EN;
#endif
	__mux_irq(irq, offset, 1);
	if (!__irq_find_base(irq, &base, &bit))
		__raw_writel(1 << bit, base + offset);
}

EXPORT_SYMBOL(sci_intc_unmask);

static void __irq_mask(struct irq_data *data)
{
	sci_intc_mask(data->irq);
}

static void __irq_unmask(struct irq_data *data)
{
	sci_intc_unmask(data->irq);
}

static int __set_wake(struct irq_data *d, unsigned int on)
{
	/*we don't add mask/unmask in this now,  this is not mean mask interrupt*/
	return 0;
}

void __init sci_init_irq(void)
{
#ifdef CONFIG_OF
	if (!of_have_populated_dt()){
#endif
		gic_init(0, 29, (void __iomem *)CORE_GIC_DIS_VA,
				(void __iomem *)CORE_GIC_CPU_VA);
		ana_init_irq();
#ifdef CONFIG_OF
	}else{
		irqchip_init();
	}
#endif
	gic_arch_extn.irq_mask = __irq_mask;
	gic_arch_extn.irq_unmask = __irq_unmask;
	gic_arch_extn.irq_set_wake = __set_wake;


	/*disable legacy interrupt*/
#if (LEGACY_FIQ_BIT < 32)
	__raw_writel(1<<LEGACY_FIQ_BIT, CORE_GIC_DIS_VA + GIC_DIST_ENABLE_CLEAR);
#endif
	__raw_writel(1<<LEGACY_IRQ_BIT, CORE_GIC_DIS_VA + GIC_DIST_ENABLE_CLEAR);

	__irq_init();
}
