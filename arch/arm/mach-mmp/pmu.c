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
#include <mach/features.h>
#include <mach/regs-coresight.h>

static struct platform_device pmu_device = {
	.name		= "arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
};

static struct resource pmu_resource_pxa[] = {
#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
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
#if defined(CONFIG_CPU_PXA1088)
	/* core2 */
	{
		.start	= IRQ_PXA1088_CORESIGHT3,
		.end	= IRQ_PXA1088_CORESIGHT3,
		.flags	= IORESOURCE_IRQ,
	},
	/* core3 */
	{
		.start	= IRQ_PXA1088_CORESIGHT4,
		.end	= IRQ_PXA1088_CORESIGHT4,
		.flags	= IORESOURCE_IRQ,
	},
#endif
#endif
};

#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
static void __init pxa988_enable_external_agent(void __iomem *addr)
{
	u32 tmp;

	tmp = readl_relaxed(addr);
	tmp |= 0x100000;
	writel_relaxed(tmp, addr);
}

static void __init pxa988_cti_enable(u32 cpu)
{
	void __iomem *cti_base = CTI_CORE0_VIRT_BASE + 0x1000 * cpu;
	u32 tmp;

	/* Unlock CTI */
	writel_relaxed(0xC5ACCE55, cti_base + CTI_LOCK_OFFSET);

	/*
	 * Enables a cross trigger event to the corresponding channel.
	 */
	tmp = readl_relaxed(cti_base + CTI_EN_IN1_OFFSET);
	tmp &= ~CTI_EN_MASK;
	tmp |= 0x1 << cpu;
	writel_relaxed(tmp, cti_base + CTI_EN_IN1_OFFSET);

	tmp = readl_relaxed(cti_base + CTI_EN_OUT6_OFFSET);
	tmp &= ~CTI_EN_MASK;
	tmp |= 0x1 << cpu;
	writel_relaxed(tmp, cti_base + CTI_EN_OUT6_OFFSET);

	/* Enable CTI */
	writel_relaxed(0x1, cti_base + CTI_CTRL_OFFSET);
}

static void __init pxa988_cti_init(void)
{
	int cpu;

	/* if enable TrustZone, move core config to TZSW. */
#ifndef CONFIG_TZ_HYPERVISOR
	/* enable access CTI registers for core */
	pxa988_enable_external_agent(CIU_CPU_CORE0_CONF);
	pxa988_enable_external_agent(CIU_CPU_CORE1_CONF);
#if defined(CONFIG_CPU_PXA1088)
	pxa988_enable_external_agent(CIU_CPU_CORE2_CONF);
	pxa988_enable_external_agent(CIU_CPU_CORE3_CONF);
#endif
#endif /* CONFIG_TZ_HYPERVISOR */

	for (cpu = 0; cpu < CONFIG_NR_CPUS; cpu++)
		pxa988_cti_enable(cpu);
}

void pxa988_ack_ctiint(void)
{
	writel_relaxed(0x40, CTI_REG(CTI_INTACK_OFFSET));
}
EXPORT_SYMBOL(pxa988_ack_ctiint);

void pxa_pmu_ack(void)
{
	pxa988_ack_ctiint();
}
#endif

static int __init pxa_pmu_init(void)
{
	if (has_feat_pmu_support()) {
		pmu_device.resource = pmu_resource_pxa;
		pmu_device.num_resources = ARRAY_SIZE(pmu_resource_pxa);

#if defined(CONFIG_CPU_PXA988) || defined(CONFIG_CPU_PXA1088)
		pxa988_cti_init();
#endif
		platform_device_register(&pmu_device);
	} else {
		printk(KERN_WARNING "unsupported Soc for PMU");
		return -EIO;
	}
	return 0;
}
arch_initcall(pxa_pmu_init);
