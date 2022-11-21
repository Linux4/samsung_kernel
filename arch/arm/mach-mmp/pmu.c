/*
 * PMU IRQ registration for MMP PMU families.
 *
 * (C) Copyright 2011 Marvell International Ltd.
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <asm/pmu.h>
#include <asm/io.h>
#include <mach/addr-map.h>
#include <mach/cputype.h>
#include <mach/irqs.h>
#include <mach/regs-ciu.h>

#ifdef CONFIG_CPU_PXA988
#include <mach/regs-coresight.h>
#endif

static struct platform_device pmu_device = {
	.name		= "arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
};

#ifdef CONFIG_CPU_PXA988
static struct resource pmu_resource_pxa988[] = {
	/* core0 */
	{
		.start	= IRQ_PXA988_CORESIGHT,
		.end	= IRQ_PXA988_CORESIGHT,
		.flags	= IORESOURCE_IRQ,
	},
	/* core1*/
	{
		.start	= IRQ_PXA988_CORESIGHT2,
		.end	= IRQ_PXA988_CORESIGHT2,
		.flags	= IORESOURCE_IRQ,
	},
};

static void __init pxa988_cti_init(void)
{
	u32 tmp;

	/* enable access CTI registers for core0 */
	tmp = __raw_readl(CIU_CA9_CPU_CORE0_CONF);
	tmp |= 0x100000;
	__raw_writel(tmp, CIU_CA9_CPU_CORE0_CONF);

	/* enable access CTI registers for core1 */
	tmp = __raw_readl(CIU_CA9_CPU_CORE1_CONF);
	tmp |= 0x100000;
	__raw_writel(tmp, CIU_CA9_CPU_CORE1_CONF);

	/* enable the write access to CTI0 */
	__raw_writel(0xC5ACCE55, CTI_CORE0_VIRT_BASE + CTI_LOCK_OFFSET);
	__raw_writel(0x1, CTI_CORE0_VIRT_BASE + CTI_CTRL_OFFSET);

	/*
	 * enable core0 CTI triger in1 from PMU0 irq to CTM channel 0
	 * and enable the CTM channel 0 route to core0 CTI trigger out 6
	 */
	tmp = __raw_readl(CTI_CORE0_VIRT_BASE + CTI_EN_IN1_OFFSET);
	tmp &= ~CTI_EN_MASK;
	tmp |= 0x1;
	__raw_writel(tmp, CTI_CORE0_VIRT_BASE + CTI_EN_IN1_OFFSET);

	tmp = __raw_readl(CTI_CORE0_VIRT_BASE + CTI_EN_OUT6_OFFSET);
	tmp &= ~CTI_EN_MASK;
	tmp |= 0x1;
	__raw_writel(tmp, CTI_CORE0_VIRT_BASE + CTI_EN_OUT6_OFFSET);


	/* enable the write access to CTI1 */
	__raw_writel(0xC5ACCE55, CTI_CORE1_VIRT_BASE + CTI_LOCK_OFFSET);
	__raw_writel(0x1, CTI_CORE1_VIRT_BASE + CTI_CTRL_OFFSET);

	/*
	 * enable core1 CTI triger in1 from PMU1 irq to CTM channel 1
	 * and enable the CTM channel 1 route to core1 CTI trigger out 6
	 */
	tmp = __raw_readl(CTI_CORE1_VIRT_BASE + CTI_EN_IN1_OFFSET);
	tmp &= ~CTI_EN_MASK;
	tmp |= 0x2;
	__raw_writel(tmp, CTI_CORE1_VIRT_BASE + CTI_EN_IN1_OFFSET);

	tmp = __raw_readl(CTI_CORE1_VIRT_BASE + CTI_EN_OUT6_OFFSET);
	tmp &= ~CTI_EN_MASK;
	tmp |= 0x2;
	__raw_writel(tmp, CTI_CORE1_VIRT_BASE + CTI_EN_OUT6_OFFSET);
}

void pxa988_ack_ctiint(void)
{
	/* ack the cti irq */
	__raw_writel(0x40, CTI_REG(CTI_INTACK_OFFSET));
}
EXPORT_SYMBOL(pxa988_ack_ctiint);
#endif

static int __init pxa_pmu_init(void)
{
#ifdef CONFIG_CPU_PXA988
	pmu_device.resource = pmu_resource_pxa988;
	pmu_device.num_resources = ARRAY_SIZE(pmu_resource_pxa988);

	/* Need to init CTI irq line */
	pxa988_cti_init();
#endif

	if (pmu_device.resource) {
		platform_device_register(&pmu_device);
	} else {
		printk(KERN_WARNING "unsupported Soc for PMU");
		return -EIO;
	}
	return 0;
}
arch_initcall(pxa_pmu_init);
