/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 * Author: steve.zhan <steve.zhan@spreadtrum.com>
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/interrupt.h>

#include <asm/delay.h>

#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/adi.h>
#include <mach/irqs.h>

/* registers definitions for controller ANA_CTL_INT */
#define ANA_REG_INT_MASK_STATUS         (ANA_CTL_INT_BASE + 0x0000)
#define ANA_REG_INT_RAW_STATUS          (ANA_CTL_INT_BASE + 0x0004)
#define ANA_REG_INT_EN                  (ANA_CTL_INT_BASE + 0x0008)
#define ANA_REG_INT_MASK_STATUS_SYNC    (ANA_CTL_INT_BASE + 0x000c)

/* bits definitions for register REG_INT_MASK_STATUS */
#if defined(CONFIG_ARCH_SC8825)
/* vars definitions for controller ANA_CTL_INT */
#define MASK_ANA_INT                    ( 0xFF )
#elif defined(CONFIG_ARCH_SCX15)
/* vars definitions for controller ANA_CTL_INT */
#define MASK_ANA_INT                    ( 0x3FF )
#elif defined(CONFIG_ARCH_SCX35)
/* vars definitions for controller ANA_CTL_INT */
#define MASK_ANA_INT                    ( 0x7FF )
#else
#error "pls define interrupt number mask value"
#endif


void sprd_ack_ana_irq(struct irq_data *data)
{
	/* nothing to do... */
}

static void sprd_mask_ana_irq(struct irq_data *data)
{
	int offset = data->irq - IRQ_ANA_INT_START;
	pr_debug("%s %d\n", __FUNCTION__, data->irq);
	sci_adi_write(ANA_REG_INT_EN, 0, BIT(offset) & MASK_ANA_INT);
}

static void sprd_unmask_ana_irq(struct irq_data *data)
{
	int offset = data->irq - IRQ_ANA_INT_START;
	pr_debug("%s %d\n", __FUNCTION__, data->irq);
	sci_adi_write(ANA_REG_INT_EN, BIT(offset) & MASK_ANA_INT, 0);
}

static struct irq_chip sprd_muxed_ana_chip = {
	.name = "irq-ANA",
	.irq_ack = sprd_ack_ana_irq,
	.irq_mask = sprd_mask_ana_irq,
	.irq_unmask = sprd_unmask_ana_irq,
};

static irqreturn_t sprd_muxed_ana_handler(int irq, void *dev_id)
{
	uint32_t irq_ana, status;
	int i;

	status = sci_adi_read(ANA_REG_INT_MASK_STATUS) & MASK_ANA_INT;
	pr_debug("%s %d 0x%08x\n", __FUNCTION__, irq, status);
	while (status) {
		i = __ffs(status);
		status &= ~(1 << i);
		irq_ana = IRQ_ANA_INT_START + i;
		pr_debug("%s generic_handle_irq %d\n", __FUNCTION__, irq_ana);
		generic_handle_irq(irq_ana);
	}
	return IRQ_HANDLED;
}

static struct irqaction __adie_mux_irq = {
	.name		= "adie_mux",
	.flags		= IRQF_DISABLED | IRQF_NO_SUSPEND,
	.handler	= sprd_muxed_ana_handler,
};

void __init ana_init_irq(void)
{
	int n;

	for (n = IRQ_ANA_INT_START; n < IRQ_ANA_INT_START + NR_ANA_IRQS; n++) {
		irq_set_chip_and_handler(n, &sprd_muxed_ana_chip,
					 handle_level_irq);
		set_irq_flags(n, IRQF_VALID);
	}
	setup_irq(IRQ_ANA_INT, &__adie_mux_irq);

}

